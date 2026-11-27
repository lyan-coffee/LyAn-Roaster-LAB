/*
 * lib/RoasterSim.h - Temperature simulator (tier 3)
 *
 * With no real temperature signal, the user can enable simulation to test Heater, Air, PID, HiBean, Artisan... without a roaster.
 * Thermal model calibrated on Skywalker V1 logs:
 *     dT/dt = a * Heater - k * (T - T_ambient),  k grows with Air (and Cool), in VIRTUAL time
 *   Heater 100%, Air 50%: 30 -> 175 °C takes ~7 virtual min (~1 real min at x7). Holding 175 °C needs Heater ~35%.
 *   Cool on: fast cooling (~4 virtual min to near 60 °C). Charge: drum temp drops sharply, probe follows (TP curve).
 *   The probe (BT) lags the drum temperature by ~25 virtual s.
 *
 * Two parts:
 *   A. Model + interface (simInit / simTick / simGetBT / simGetET): needs only Arduino.h, RoasterSetting.h, RoasterSync.h, RoasterState.h.
 *   B. V3 glue (simUpdate): compiled only when RoasterSetting.h defines ROASTER_V3:
 *      real <-> simulated temperature handover, virtual clock bclock() x SIM_TIME_SCALE, no frames sent to a real roaster while simulating.
 *
 * Simulation rules:
 *   - Runs only without a real signal. A real signal (within 3 s) turns the simulator off and real temperature is used.
 *   - While simulating, no control frames go to the roaster (offline); at simulation START one "all off" frame is sent
 *     so a connected real roaster goes safe (simUpdate() returns true then; loop() calls machineSendOff()).
 *   - Simulated temperature feeds PID and HiBean READ replies like a real one.
 *   - The simulation state is not saved: reboot = off (unless DEFAULT_SIMULATION = 1).
 *   - ENABLE_SIMULATION 0 removes the feature (empty functions only).
 *
 * Depends on: RoasterSetting.h (SIM_AMBIENT, SIM_SCALE_DEFAULT, ENABLE_SIMULATION), RoasterSync.h (LOCK, bclock, g_timeScale), RoasterState.h
 * (sendBuffer, g_state, g_simActive, g_simWanted, temp, lastTempMs); V3 adds RoasterBatch.h (realSignalOk, batchCharged).
 * ----------------------------------------------------------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"
#include "RoasterState.h"
#ifdef ROASTER_V3
  #include "RoasterBatch.h"
#endif

#if ENABLE_SIMULATION
#define HAS_MODULE_SIM 1

// =============================================================================
// A. THERMAL MODEL
// =============================================================================
// Simulation runs SIM_TIME_SCALE times faster than real time (7 min real preheat -> ~1 min). RoR is converted to real time
static uint8_t g_simScale = SIM_SCALE_DEFAULT;        // current factor (the user changes it in Setting: x1 x3 x5 x7; saved to NVS in RoasterSettings.h)
#define SIM_TIME_SCALE ((float)g_simScale)

static float simTemp  = SIM_AMBIENT;                  // drum temperature (°C)
static float simProbe = SIM_AMBIENT;                  // BT probe temperature (lags drum temp by ~30 virtual s)
static float simEt    = SIM_AMBIENT;                  // ET derived from BT (no separate ET probe)

static void simInit() {
    simTemp = simProbe = simEt = SIM_AMBIENT;
}

// One model step. heat, air: 0..100 (%). cool: cooling fan. dt: VIRTUAL seconds of the step (V3: 0.1 * SIM_TIME_SCALE per 100 ms)
// Heater 100%, Air 50%: 30 -> 175 °C takes ~7 virtual min (~1 min real at SIM_TIME_SCALE = 7),
// max RoR ~27 °C/min (virtual time) at the start of heating, then falling like a real roaster.
// Holding 175 °C needs Heater ~35%. Cool on: fast cooling (~4 virtual min to near 60 °C).
static void simStep(float heat, float air, bool cool, float dt) {
    const float heat_in = heat * 0.0042f;                                // °C / virtual s
    const float k = 0.00086f + 0.000003f * air + (cool ? 0.005f : 0.0f);  // heat-loss coefficient (1/virtual s)
    simTemp += (heat_in - (simTemp - SIM_AMBIENT) * k) * dt;
    simTemp = constrain(simTemp, 0.0f, (float)300);
    simProbe += (simTemp - simProbe) * fminf(1.0f, dt / 30.0f);          // probe lags: after Charge BT drops then rises again (TP)
}

// Charge (beans in): bean temp drops sharply like a real roaster (TP) - for graph / TP testing only
static void simOnCharge() {
    if (simTemp > 120.0f) simTemp = SIM_AMBIENT + (simTemp - SIM_AMBIENT) * 0.30f;
}

// BT for PID / HiBean: probe + noise (not accumulated into state)
static float simGetBT() { return simProbe + random(-3, 4) * 0.02f; }

// =============================================================================
// B. V3 GLUE
// =============================================================================
#ifdef ROASTER_V3

static bool s_simLastCharged = false;           // already charged at the previous step (to trigger exactly at Charge)

// One glue step. Returns true IF simulation just started (loop() must call machineSendOff() to put the real roaster off).
static bool simGlue(uint32_t now) {
    const bool real = realSignalOk();
    bool justStarted = false;

    LOCK();
    if (g_simActive && real) {
        // Real signal present: yield to real temperature and stop simulation
        g_simActive = false;
        bclock(); g_timeScale = 1.0f;
        g_simWanted = false;
    } else if (g_simWanted && !real) {
        if (!g_simActive) {
            g_simActive = true;
            bclock(); g_timeScale = SIM_TIME_SCALE;
            simInit();
            justStarted = true;
        }
        if (batchCharged && !s_simLastCharged) simOnCharge();
        s_simLastCharged = batchCharged;
        const float dt = 0.1f * SIM_TIME_SCALE;                          // virtual seconds per step
        simStep(sendBuffer[HEAT_BYTE], sendBuffer[VENT_BYTE], sendBuffer[COOL_BYTE] > 0, dt);
        if (g_timeScale != SIM_TIME_SCALE) { bclock(); g_timeScale = SIM_TIME_SCALE; }   // change factor while running
        temp = simGetBT();
        lastTempMs = now;                                                // not treated as signal lost
    } else if (g_simActive) {
        g_simActive = false;                                             // user turned simulation off
        bclock(); g_timeScale = 1.0f;
    }
    UNLOCK();
    return justStarted;
}

// Called every loop() (core 1). Updates the thermal model every 100 ms. Returns true when simulation just started.
static bool simUpdate() {
    static uint32_t last = 0;
    const uint32_t now = millis();
    if (now - last < 100) return false;
    last = now;
    return simGlue(now);
}

#endif  // ROASTER_V3

#else   // ------------------------------------------------- ENABLE_SIMULATION 0
#ifdef ROASTER_V3
static inline bool simUpdate() { return false; }
#endif
#endif  // ENABLE_SIMULATION
