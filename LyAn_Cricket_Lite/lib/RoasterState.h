/*
 * lib/RoasterState.h - Shared state and command queue (tier 1)
 *
 * Common state board of the firmware. Modules read/write state through this file only;
 * no module calls a peer or higher-tier module directly.
 *
 *   1. sendBuffer            6-byte control frame to the roaster (single source of truth for Heater / Air / Drum / Cool)
 *   2. RoasterState g_state  temperature, RoR, simulation flag, signal loss, HiBean / Artisan connected...
 *   3. Shared extras         simulation, overheat, profile, roasting flags
 *   4. Command queue         postCommand() / popCommand() - every source (BLE, Artisan, Web, buttons) posts here
 *
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"

// =============================================================================
// 1. 6-byte control frame to the roaster: {VENT, FILTER, COOL, DRUM, HEAT, CHECK}
//    Values are percent 0..100 (Drum, Cool: 0 or 100). CHECK = sum of the first 5 bytes (set by the machine driver).
// =============================================================================
static const int CONTROLLER_LENGTH = 6;
enum BufIdx       { IDX_VENT = 0, IDX_FILTER = 1, IDX_COOL = 2, IDX_DRUM = 3, IDX_HEAT = 4, IDX_CHECK = 5 };
enum ControlBytes { VENT_BYTE = IDX_VENT, FILTER_BYTE = IDX_FILTER, COOL_BYTE = IDX_COOL,
                    DRUM_BYTE = IDX_DRUM, HEAT_BYTE = IDX_HEAT, CHECK_BYTE = IDX_CHECK };
static uint8_t sendBuffer[CONTROLLER_LENGTH] = { 0, 0, 0, 0, 0, 0 };

// =============================================================================
// 2. Overall state (modules read/write only through this struct)
// =============================================================================
struct RoasterState {
    float    bt = SIM_AMBIENT;
    float    et = SIM_AMBIENT;
    float    ror = 0.0f;
    float    pidSV = DEFAULT_PID_SETPOINT;
    bool     pidActive = false;
    bool     simMode = true;
    bool     rxFault = false;
    bool     tempUnitF = false;
    bool     hibeanLinked = false;
    bool     artisanLinked = false;
    // ---- extras ----
    bool     roastBusy = false;      // roasting (START pressed, END not yet): Web / OTA must not restart or flash
};
static RoasterState g_state;

// =============================================================================
// 3. Shared extras, so Sim, Alarm, Settings, Graph, Log, Profile can read them without including the control core
// =============================================================================
static volatile bool     g_simWanted = (DEFAULT_SIMULATION);  // user wants simulation
static volatile bool     g_simActive = false;                 // simulation running (only when there is no real signal)
// g_timeScale is in RoasterSync.h with bclock()

// Heater cut level on overheat (°C). 0 = off. RoasterAlarm refreshes it from the Setting warning level every 100 ms.
static volatile uint16_t g_overheatCutC = 0;

// ---- Bean temperature ----
// Written by the machine driver (machinePoll) or the simulator; all other modules only read. Multi-variable access must be inside LOCK().
static double   temp = 0.0;                    // BT, always °C
static char     CorF = 'C';                    // unit HiBean asked for (used only in READ replies)
static uint32_t lastTempMs = 0;                // last valid temperature (real or simulated), millis()
static uint32_t lastRealTempMs = 0;            // last REAL temperature (excludes simulation)

// Temperature signal LOST warning while heating (set by the control core, read by UI / buzzer). See RoasterSetting.h section 5.
static volatile bool g_signalLostAlarm = false;


// =============================================================================
// 4. Normalized commands from every source (BLE / Artisan / Web / buttons) -> control core
// =============================================================================
enum CmdType : uint8_t {
    CMD_NONE = 0,
    CMD_SET_HEAT,     // p1 = % Heater (0..100)
    CMD_STEP_HEAT,    // p1 = delta (+/- %)
    CMD_SET_AIR,      // p1 = % Air (0..100)
    CMD_STEP_AIR,     // p1 = delta (+/- %)
    CMD_SET_DRUM,     // p1 = 1 (ON) / 0 (OFF)
    CMD_SET_COOL,     // p1 = 1 (ON) / 0 (OFF)
    CMD_PID_ON,       // PID on
    CMD_PID_OFF,      // PID off, keep current heat level
    CMD_PID_SV,       // p1 = Setpoint (°C)
    CMD_PID_TUNE,     // p1 = Kp, p2 = Ki, p3 = Kd
    CMD_SET_UNIT_F,   // p1 = 1 (°F) / 0 (°C)
    CMD_DROP,         // cut Heater, keep Drum & Air running to cool the drum
    CMD_ESTOP         // emergency stop: cut Heater, Drum / Air / Cool 100%
};

struct RoasterCmd {
    CmdType type = CMD_NONE;
    float   p1 = 0.0f;
    float   p2 = 0.0f;
    float   p3 = 0.0f;
};

// Static 32-slot ring queue: no heap, thread-safe
#define CMD_QUEUE_SIZE 32
static RoasterCmd g_cmdQueue[CMD_QUEUE_SIZE];
static volatile uint8_t g_cmdHead = 0;
static volatile uint8_t g_cmdTail = 0;

inline bool popCommand(RoasterCmd &out) {
    bool has = false;
    LOCK();
    if (g_cmdTail != g_cmdHead) {
        out = g_cmdQueue[g_cmdTail];
        g_cmdTail = (g_cmdTail + 1) % CMD_QUEUE_SIZE;
        has = true;
    }
    UNLOCK();
    return has;
}
