/*
 * lib/RoasterBatch.h - Roast batch: events, "Time" clock, RoR, roasting flags (tier 2)
 *
 * Contents:
 *   - events PreHeat, Charge, DE, 1FC, 2FC, Drop and the Time clock (from Charge)
 *   - batchLive (batch open: START to END), roasting (roasting, not yet Drop)
 *   - RoR (°C/min): BT sampled every second on the virtual clock bclock(), compared with 30 s earlier
 *   - realSignalOk(): real temperature received in the last 3 s (used by the simulator)
 * Graph, Log, Profile, Alarm and UI read the batch through this file without the control core.
 *   - tempSignalOk() / tempSignalLost()
 *
 * Depends on: RoasterSetting.h, RoasterSync.h (LOCK, bclock), RoasterState.h (temp, lastTempMs, g_simActive).
 * ----------------------------------------------------------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"
#include "RoasterState.h"

#define HAS_MODULE_BATCH 1

// ---- Temperature signal ----
// Real temperature from the roaster in the last 3 s?
static bool realSignalOk() { return lastRealTempMs != 0 && (millis() - lastRealTempMs) < 3000; }
// A temperature frame arrived within TEMP_SIGNAL_OK_MS
static bool tempSignalOk()   { return (lastTempMs != 0) && ((millis() - lastTempMs) < TEMP_SIGNAL_OK_MS); }
// LOST: never received, or no frame for TEMP_STALE_MS (TEMP_STALE_MS = 0: check disabled, never lost)
static bool tempSignalLost() { return TEMP_STALE_MS > 0 && (lastTempMs == 0 || (millis() - lastTempMs) > TEMP_STALE_MS); }

// ---- Roast state (protected by LOCK) ----
static bool     roasting      = false;
static uint32_t roastStartMs  = 0;
static uint32_t roastFrozenMs = 0;

// ---- Batch events (PreHeat, Charge, DE = Dry End, 1FC, 2FC, Drop) + "Time" clock (from Charge) ----
enum { EV_PREHEAT = 0, EV_CHARGE, EV_DE, EV_FC1, EV_FC2, EV_DROP, EV_COUNT };
#define EV_NO_TIME 0xFFFFFFFFu             // event has no time (e.g. Drop while the Time clock is not running)
static uint8_t  evDone        = 0;           // bit i = event i pressed
static uint32_t evMs[EV_COUNT] = {0};        // event time (ms from Charge)
static float    evTemp[EV_COUNT] = {0};      // BT temperature (°C) when the event was pressed
static bool     batchCharged  = false;       // Charge pressed
static bool     batchRunning  = false;       // Time clock running
static uint32_t chargeMs      = 0;
static uint32_t batchFrozenMs = 0;
static uint32_t dropArmUntil  = 0;           // != 0: waiting for Drop confirmation until this millis()
static bool     batchLive     = false;       // batch open: START to END (includes after Drop, while cooling / reviewing the graph)
static uint32_t endArmUntil   = 0;           // != 0: waiting for END confirmation (only before Drop) until this millis()

static uint32_t batchElapsedMs() { return batchRunning ? (bclock() - chargeMs) : batchFrozenMs; }
static void batchFreeze() { if (batchRunning) { batchFrozenMs = bclock() - chargeMs; batchRunning = false; } dropArmUntil = 0; }
static void batchReset()  { evDone = 0; for (int i = 0; i < EV_COUNT; i++) { evMs[i] = 0; evTemp[i] = 0; }
                            batchCharged = batchRunning = false; chargeMs = batchFrozenMs = 0; dropArmUntil = 0; endArmUntil = 0; }

// ---- RoR (°C/min): BT sampled every second, compared with 30 s earlier, divided by g_timeScale (1 when not simulating) ----
// ROR_WINDOW_S: RoasterSetting.h
#define ROR_MIN_SAMPLES 4                  // progressive RoR starts after 3 s (4 samples), widening up to ROR_WINDOW_S (30 s)
static float    rorBuf[ROR_WINDOW_S + 1];
static uint32_t rorTime[ROR_WINDOW_S + 1]; // bclock() time (ms) of each sample, to compensate loop jitter
static int      rorHead = 0, rorCnt = 0;
static float    rorValue = 0.0f;
static bool     rorOk = false;
static uint32_t rorLastMs = 0;

static uint32_t roastElapsedMs() { return roasting ? (bclock() - roastStartMs) : roastFrozenMs; }
static void roastBegin() { roasting = true; batchLive = true; roastStartMs = bclock(); roastFrozenMs = 0; }
static void roastEnd()   { batchFreeze(); if (roasting) { roastFrozenMs = bclock() - roastStartMs; roasting = false; } }

// Reason Heater is blocked (nullptr = allowed). START is required only for commands from the screen.
static const char *needStartHint() { return batchLive ? "Press END" : "Press START"; }   // START pressed but roasting = false (Drop / Cool on): END needed

// Called every loop() (core 1); samples every 1 s of bclock() (the temperature source's time).
// At 7x simulation 1 virtual s = ~143 ms real: the 30 s window is 30 virtual s and RoR is per virtual minute.
static void rorUpdate() {
    const uint32_t vnow = bclock();                          // virtual clock: decides the sampling beat
    const uint32_t elapsed = vnow - rorLastMs;
    if (elapsed < 1000) return;
    rorLastMs = (elapsed < 2000) ? (rorLastMs + 1000) : vnow; // keep the average period at exactly 1000 ms, do not drift the 30 s window
    const uint32_t now = millis();                           // real time: checks temperature signal loss
    LOCK();
    static bool lastSim = false;
    if (g_simActive != lastSim) { lastSim = g_simActive; rorCnt = 0; rorHead = 0; rorOk = false; rorValue = 0.0f; }
    const bool ok = (lastTempMs != 0) && ((now - lastTempMs) < TEMP_SIGNAL_OK_MS);
    if (!ok) {
        rorCnt = 0; rorHead = 0; rorOk = false; rorValue = 0.0f;  // signal lost -> restart from scratch
    } else {
        const int cap = ROR_WINDOW_S + 1;
        rorBuf[rorHead]  = (float)temp;
        rorTime[rorHead] = vnow;
        rorHead = (rorHead + 1) % cap;
        if (rorCnt < cap) rorCnt++;
        // Linear regression (least squares) over the growing 3-30 s window (matches HiBean, smooths 0.1°C noise)
        if (rorCnt >= ROR_MIN_SAMPLES) {
            const int start = (rorHead + cap - rorCnt) % cap;
            const uint32_t t0 = rorTime[start];
            double sumT = 0.0, sumY = 0.0;
            for (int k = 0; k < rorCnt; k++) {
                const int idx = (start + k) % cap;
                sumT += (double)(rorTime[idx] - t0) * 0.001;
                sumY += (double)rorBuf[idx];
            }
            const double tMean = sumT / (double)rorCnt;
            const double yMean = sumY / (double)rorCnt;
            double sxx = 0.0, sxy = 0.0;
            for (int k = 0; k < rorCnt; k++) {
                const int idx = (start + k) % cap;
                const double dt = (double)(rorTime[idx] - t0) * 0.001 - tMean;
                const double dy = (double)rorBuf[idx] - yMean;
                sxx += dt * dt;
                sxy += dt * dy;
            }
            if (sxx > 1e-6) {
                rorValue = (float)((sxy / sxx) * 60.0);       // °C / min (virtual minutes when simulating)
                rorOk = true;
            } else {
                rorOk = false;
            }
        } else {
            rorOk = false;
        }
    }
    UNLOCK();
}
