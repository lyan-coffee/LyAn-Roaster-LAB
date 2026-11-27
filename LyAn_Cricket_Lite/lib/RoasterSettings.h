/*
 * lib/RoasterSettings.h - Persistent User & PID Settings via NVS (Tier 4)
 *
 * Stores display unit, language, overheat cutoff, simulation speed, and standard PID
 * tunings (Kp, Ki, Kd, P-Mode) in the "lygia" NVS namespace.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "../RoasterSetting.h"
#include "UIConfig.h"
#include "RoasterSync.h"
#include "RoasterState.h"
#include "RoasterPID.h"
#include "RoasterSim.h"

#define HAS_MODULE_SETTINGS 1

enum : uint8_t { UNIT_C = 0, UNIT_F = 1 };

struct RoasterSettings {
    uint8_t  unit;
    uint16_t warn;     // Overheat cutoff threshold (C)
};

static uint16_t warn_clamp(int v) {
    v = ((v + WARN_TEMP_STEP / 2) / WARN_TEMP_STEP) * WARN_TEMP_STEP;
    if (v < WARN_TEMP_MIN) v = WARN_TEMP_MIN;
    if (v > WARN_TEMP_MAX) v = WARN_TEMP_MAX;
    return (uint16_t)v;
}

static RoasterSettings g_set = {
    DEFAULT_TEMP_UNIT,
    (uint16_t)(DEFAULT_WARN_TEMP < WARN_TEMP_MIN ? WARN_TEMP_MIN : (DEFAULT_WARN_TEMP > WARN_TEMP_MAX ? WARN_TEMP_MAX : DEFAULT_WARN_TEMP))
};

// ---- Standard PID Parameters (NVS keys: "pkp", "pki", "pkd", "ppm") ----
#if ENABLE_SETTINGS_PAGE && ENABLE_PID_SETTINGS
static void pidCfgApply(float kp, float ki, float kd, uint8_t pm) {
    LOCK();
    myPIDConfig.setKp(kp);
    myPIDConfig.setKi(ki);
    myPIDConfig.setKd(kd);
    myPIDConfig.setPMode(pm ? P_ON_M : P_ON_E);
    myPIDConfig.apply(myPID);
    UNLOCK();
}

static void pidSettingsSave() {
    Preferences p;
    if (p.begin("lygia", false)) {
        p.putFloat("pkp", (float)myPIDConfig.getKp());
        p.putFloat("pki", (float)myPIDConfig.getKi());
        p.putFloat("pkd", (float)myPIDConfig.getKd());
        p.putUChar("ppm", myPIDConfig.getPMode() == P_ON_M ? 1 : 0);
        p.end();
    }
}

static void pidSettingsResetDefault() {
    pidCfgApply((float)PID_DEFAULT_KP, (float)PID_DEFAULT_KI, (float)PID_DEFAULT_KD,
                (uint8_t)(PID_DEFAULT_PMODE ? 0 : 1));
    pidSettingsSave();
}

static void pidSettingsLoad() {
    Preferences p;
    if (!p.begin("lygia", false)) return;
    if (p.isKey("pkp")) {
        const float   kp = p.getFloat("pkp", (float)PID_DEFAULT_KP);
        const float   ki = p.getFloat("pki", (float)PID_DEFAULT_KI);
        const float   kd = p.getFloat("pkd", (float)PID_DEFAULT_KD);
        const uint8_t pm = p.getUChar("ppm", (uint8_t)(PID_DEFAULT_PMODE ? 0 : 1));
        p.end();
        if (kp >= 0 && kp <= 60 && ki >= 0 && ki <= 5 && kd >= 0 && kd <= 200) {
            pidCfgApply(kp, ki, kd, pm ? 1 : 0);
        }
    } else {
        p.end();
    }
}
#endif

// Push unit and overheat threshold down to the control core
static void settingsPushToCore() {
    g_state.tempUnitF = (g_set.unit == UNIT_F);
    g_overheatCutC = ENABLE_OVERHEAT_ALARM ? g_set.warn : (uint16_t)0;
}

static void settingsLoad() {
#if ENABLE_SETTINGS_PAGE
    Preferences p;
    if (p.begin("lygia", false)) {
        g_set.unit   = p.getUChar("unit", DEFAULT_TEMP_UNIT);
        g_set.warn   = p.getUShort("warn", DEFAULT_WARN_TEMP);
#if ENABLE_SIMULATION
        g_simScale = p.getUChar("simx", SIM_SCALE_DEFAULT);
#endif
        p.end();
    }
    if (g_set.unit   > UNIT_F)        g_set.unit   = DEFAULT_TEMP_UNIT;
    g_set.warn = warn_clamp(g_set.warn);
#if ENABLE_PID_SETTINGS
    pidSettingsLoad();
#endif
#if ENABLE_SIMULATION
    if (g_simScale != 1 && g_simScale != 3 && g_simScale != 5 && g_simScale != 7) g_simScale = SIM_SCALE_DEFAULT;
#endif
#endif
    settingsPushToCore();
}

static void settingsSave() {
    settingsPushToCore();
#if ENABLE_SETTINGS_PAGE
    Preferences p;
    if (p.begin("lygia", false)) {
        p.putUChar("unit", g_set.unit);
        p.putUShort("warn", g_set.warn);
#if ENABLE_SIMULATION
        p.putUChar("simx", g_simScale);
#endif
        p.end();
    }
#endif
}
