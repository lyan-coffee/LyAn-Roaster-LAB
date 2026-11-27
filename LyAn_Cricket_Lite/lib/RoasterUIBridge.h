/*
 * lib/RoasterUIBridge.h - Clean Bridge Between LVGL UI and Control Core (Tier 1)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <Arduino.h>
#include "UIConfig.h"

struct UiRoastSnap {
    bool     batchLive        = false;
    bool     simMode          = false;  // true = SIM, false = LIVE
    float    bt               = 0.0f;
    float    ror              = 0.0f;
    int      heaterPct        = 0;
    int      airPct           = 0;
    bool     pidOn            = false;
    float    pidSv            = 180.0f;
    bool     drumOn           = false;
    bool     coolOn           = false;
    uint32_t roastSec         = 0;

    // 6 roast events: 0=PR, 1=CH, 2=DE, 3=1FC, 4=2FC, 5=DR
    bool     evMarked[6]      = {false, false, false, false, false, false};
    uint32_t evSec[6]         = {0, 0, 0, 0, 0, 0};
    uint32_t devSec           = 0;      // Development time since 1FC (frozen at DR)
    float    devTempDelta     = 0.0f;   // +C since 1FC
    float    devDtrPct        = 0.0f;   // % DTR since 1FC

    // Network & link mode: 0=HiBean BLE, 1=Artisan WS:81, 2=Web Portal HTTP:80, 3=Standalone
    uint8_t  linkMode         = 0;
    bool     bleConnected     = false;
    bool     artisanConnected = false;
    bool     webConnected     = false;
    bool     wifiApMode       = false;  // true = AP, false = STA
    bool     webPortalOn      = false;
    char     ipStr[24]        = "0.0.0.0";
    bool     clockSynced      = false;
    char     clockStr[8]      = "";     // HH:MM

    // System settings
    uint16_t overheatC        = 240;    // Always stored in C (220..260, step 5)
    bool     unitF            = false;  // false = C, true = F
    uint8_t  idleMin          = UI_IDLE_DEFAULT_MIN;
    uint8_t  brightnessPct    = UI_BL_DEFAULT_PCT;
    uint8_t  volumePct        = UI_VOL_DEFAULT_PCT;
    bool     soundMuted       = false;

    // Battery
    int      batPct           = 100;
    bool     batCharging      = false;

    // Safety alarms
    bool     overheatAlert    = false;
    bool     overheatCooled   = true;
    bool     signalLost       = false;
    uint32_t signalLostSec    = 0;
};

struct UiPidParams {
    float   kp = 12.0f;
    float   ki = 0.30f;
    float   kd = 20.0f;
    uint8_t pm = 0;     // 0 = P_ON_E, 1 = P_ON_M
};

struct UiBridgeOps {
    void (*readSnapshot)(UiRoastSnap &out)            = nullptr;
    void (*startManual)()                             = nullptr;
    void (*endBatch)()                                = nullptr;
    void (*setHeaterPct)(int pct)                     = nullptr;
    void (*setAirPct)(int pct)                        = nullptr;
    void (*setPidOn)(bool on)                         = nullptr;
    void (*setPidSv)(float tempC)                     = nullptr;
    void (*toggleDrum)()                              = nullptr;
    void (*toggleCool)()                              = nullptr;
    void (*markEvent)(uint8_t evIdx)                  = nullptr; // 0..5 = PR,CH,DE,1FC,2FC,DR
    void (*cycleLinkMode)()                           = nullptr;
    void (*setWifiApMode)(bool apMode)                = nullptr;
    void (*setWebPortal)(bool on)                     = nullptr;
    void (*readPidParams)(UiPidParams &out)           = nullptr;
    void (*savePidParams)(const UiPidParams &in)      = nullptr;
    void (*resetPidParams)(UiPidParams &out)          = nullptr;
    void (*setOverheatC)(uint16_t tempC)              = nullptr;
    void (*setIdleMin)(uint8_t mins)                  = nullptr;
    void (*setBrightnessPct)(uint8_t pct)             = nullptr;
    void (*setVolumePct)(uint8_t pct, bool persist)   = nullptr;
    void (*setSoundMuted)(bool muted)                 = nullptr;
    void (*setTempUnitF)(bool useF)                   = nullptr;
    void (*ackOverheat)()                             = nullptr;
    void (*systemReset)()                             = nullptr;
    void (*systemPowerOff)()                          = nullptr;
    void (*finishBootToSolo)()                        = nullptr;
    void (*ackSignalLost)()                           = nullptr;
};

inline UiBridgeOps g_uiOps;
inline UiRoastSnap g_uiSnap;

// Display unit conversion helpers (core always computes in Celsius)
static inline float       uiTempDisp(float c)      { return g_uiSnap.unitF ? (c * 1.8f + 32.0f) : c; }
static inline float       uiRorDisp(float cPerMin) { return g_uiSnap.unitF ? (cPerMin * 1.8f) : cPerMin; }
static inline const char* uiTempUnit()             { return g_uiSnap.unitF ? "°F" : "°C"; }

static inline void uiBridgeRefresh() {
    if (g_uiOps.readSnapshot) {
        g_uiOps.readSnapshot(g_uiSnap);
    }
}
