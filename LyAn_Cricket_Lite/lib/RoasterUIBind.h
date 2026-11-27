/*
 * lib/RoasterUIBind.h - Binds RoasterUIBridge (UI 240x240 v7) to the V3 core + Driver v1.2.0 (tier 7)
 */
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"
#include "RoasterState.h"
#include "RoasterClock.h"
#include "RoasterBatch.h"
#include "RoasterPID.h"
#include "RoasterControl.h"
#include "RoasterSettings.h"
#include "RoasterRadio.h"
#include "RoasterNet.h"
#include "RoasterWeb.h"
#include "RoasterDisplay.h"
#include "RoasterAlarmSound.h"       // g_alarm_sound: cricket chirp before power off / deep sleep
#if __has_include(<driver/rtc_io.h>)
  #include <esp_sleep.h>
  #include <driver/gpio.h>
  #include <driver/rtc_io.h>
  #define BIND_HAS_DEEPSLEEP 1
#endif
#if ENABLE_BATTERY
  #include "RoasterBattery.h"
#endif
#include "UILang.h"
#include "RoasterUIBridge.h"

static bool              g_bindOverheatMuted  = false;  // Muted until BT < (threshold - ALARM_HYST_C)
static bool              g_bindOverheatActive = false;  // Overheat alert currently displayed
static uint32_t          g_bindSigLostSince  = 0;
static uint32_t          g_bindPrStartMs     = 0;
static uint32_t          g_bindPrDurSec      = 0;
static bool              g_bindPrFrozen      = false;
static uint8_t           g_bindIdleMin       = UI_IDLE_DEFAULT_MIN;
static bool              g_bindIdleLoaded    = false;
static uint8_t           g_bindBlPct         = UI_BL_DEFAULT_PCT;
static uint8_t           g_bindVolPct        = UI_VOL_DEFAULT_PCT;   // master volume (chirps)
static bool              g_bindMuted         = false;                // muted
static bool              g_bindSndLoaded     = false;
static bool              g_bindBlLoaded      = false;
static bool              g_bindBootSoloDone  = false;

static void bindLoadIdleMin() {
    if (g_bindIdleLoaded) return;
    g_bindIdleLoaded = true;
    Preferences prefs;
    if (prefs.begin(UI_NVS_NS, true)) {
        uint8_t m = prefs.getUChar(UI_NVS_KEY_IDLE_MIN, UI_IDLE_DEFAULT_MIN);
        prefs.end();
        if (m == 0 || m == 1 || m == 3 || m == 5 || m == 10 || m == 15) {
            g_bindIdleMin = m;
        }
    }
}

static void bindSetIdleMin(uint8_t mins) {
    if (mins != 0 && mins != 1 && mins != 3 && mins != 5 && mins != 10 && mins != 15) {
        mins = UI_IDLE_DEFAULT_MIN;
    }
    g_bindIdleMin = mins;
    g_bindIdleLoaded = true;
    Preferences prefs;
    if (prefs.begin(UI_NVS_NS, false)) {
        prefs.putUChar(UI_NVS_KEY_IDLE_MIN, mins);
        prefs.end();
    }
}


static uint8_t bindSnapBlPct(uint8_t pct) {
    static const uint8_t kBlSteps[5] = {50, 63, 75, 88, 100};
    uint8_t best = kBlSteps[2];
    int bestDiff = 999;
    for (int i = 0; i < 5; ++i) {
        int d = abs(static_cast<int>(pct) - static_cast<int>(kBlSteps[i]));
        if (d < bestDiff) { bestDiff = d; best = kBlSteps[i]; }
    }
    return best;
}

static void bindLoadBrightness() {
    if (g_bindBlLoaded) return;
    g_bindBlLoaded = true;
    Preferences prefs;
    if (prefs.begin(UI_NVS_NS, true)) {
        uint8_t p = prefs.getUChar(UI_NVS_KEY_BL_PCT, UI_BL_DEFAULT_PCT);
        prefs.end();
        g_bindBlPct = bindSnapBlPct(p);
    }
    displayBacklightPct(g_bindBlPct);
}

static void bindSetBrightness(uint8_t pct) {
    g_bindBlPct = bindSnapBlPct(pct);
    g_bindBlLoaded = true;
    displayBacklightPct(g_bindBlPct);
    Preferences prefs;
    if (prefs.begin(UI_NVS_NS, false)) {
        prefs.putUChar(UI_NVS_KEY_BL_PCT, g_bindBlPct);
        prefs.end();
    }
}

// ---- Master volume + Mute (chirps; safety alarms unaffected): saved in NVS "lygia" ----
static void bindApplySound() { g_alarmCueVol = g_bindMuted ? 0 : g_bindVolPct; }
static void bindLoadSound() {
    if (g_bindSndLoaded) return;
    g_bindSndLoaded = true;
    Preferences prefs;
    if (prefs.begin(UI_NVS_NS, true)) {
        uint8_t v = prefs.getUChar(UI_NVS_KEY_VOL_PCT, UI_VOL_DEFAULT_PCT);
        if (v < UI_VOL_MIN_PCT) v = UI_VOL_MIN_PCT;
        if (v > 100) v = 100;
        g_bindVolPct = v;
        g_bindMuted  = prefs.getBool(UI_NVS_KEY_MUTE, false);
        prefs.end();
    }
    bindApplySound();
}
static void bindSetVolume(uint8_t pct, bool persist) {
    g_bindVolPct = pct < UI_VOL_MIN_PCT ? (uint8_t)UI_VOL_MIN_PCT : (pct > 100 ? (uint8_t)100 : pct);
    g_bindSndLoaded = true;
    bindApplySound();
    if (!persist) return;
    Preferences prefs;
    if (prefs.begin(UI_NVS_NS, false)) { prefs.putUChar(UI_NVS_KEY_VOL_PCT, g_bindVolPct); prefs.end(); }
}
static void bindSetMuted(bool muted) {
    g_bindMuted = muted;
    g_bindSndLoaded = true;
    bindApplySound();
    Preferences prefs;
    if (prefs.begin(UI_NVS_NS, false)) { prefs.putBool(UI_NVS_KEY_MUTE, g_bindMuted); prefs.end(); }
}

static void bindSetTempUnitF(bool useF) {
    g_set.unit = useF ? UNIT_F : UNIT_C;
    settingsSave(); // settingsPushToCore() pushes g_state.tempUnitF to the core (PR mark 180°C / 356°F) then saves NVS "unit"
}

static void bindSetOverheatC(uint16_t tempC) {
    g_set.warn = warn_clamp(static_cast<int>(tempC));
    settingsSave(); // applied at once to g_overheatCutC and saved in NVS "lygia"
}

static int bindBatteryPct() {
#if ENABLE_BATTERY
    const int mv = batteryMv();
    if (mv <= 0) return 100;
    int pct = (mv - UI_BAT_MV_EMPTY) * 100 / (UI_BAT_MV_FULL - UI_BAT_MV_EMPTY);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
#else
    return 100;
#endif
}

static bool bindBatteryChg() {
#if ENABLE_BATTERY
    return (batteryChargePin() == LOW);
#else
    return false;
#endif
}

static void bindReadSnapshot(UiRoastSnap &out) {
    bindLoadIdleMin();
    bindLoadBrightness();
    bindLoadSound();
    Snap s;
    roasterSnapshot(s);

    out.batchLive = s.live;
    out.simMode   = s.sim;   // Snap.sim = g_simActive (simulation really running; stops by itself when a roaster signal arrives)
    out.bt        = s.temp;
    out.ror       = s.rorOk ? s.ror : 0.0f;
    out.heaterPct = s.heat;
    out.airPct    = s.vent;
    out.pidOn     = s.pid;
    out.pidSv     = s.sp;
    out.drumOn    = (s.drum > 0);
    out.coolOn    = (s.cool > 0);
    if (!s.live) {
        g_bindPrStartMs = 0;
        g_bindPrDurSec  = 0;
        g_bindPrFrozen  = false;
        out.roastSec = 0;
    } else if (s.charged) {
        if (!g_bindPrFrozen) {
            if (g_bindPrStartMs != 0) {
                g_bindPrDurSec = (bclock() - g_bindPrStartMs) / 1000UL;
            }
            g_bindPrFrozen = true;
        }
        out.roastSec = s.batchMs / 1000UL; // counted from Charge
    } else if (s.evDone & (1u << EV_PREHEAT)) {
        if (g_bindPrStartMs == 0) g_bindPrStartMs = bclock();
        g_bindPrDurSec = (bclock() - g_bindPrStartMs) / 1000UL;
        out.roastSec   = g_bindPrDurSec;   // counted during Preheat
    } else {
        g_bindPrStartMs = 0;
        g_bindPrDurSec  = 0;
        g_bindPrFrozen  = false;
        out.roastSec = 0;
    }

    for (int i = 0; i < 6; i++) {
        out.evMarked[i] = ((s.evDone & (1u << i)) != 0);
        out.evSec[i]    = (s.evMs[i] != EV_NO_TIME) ? (s.evMs[i] / 1000UL) : 0;
    }
    // PR cell: records Preheat time (counting while PR, frozen when CH is pressed)
    if (out.evMarked[EV_PREHEAT]) {
        out.evSec[EV_PREHEAT] = g_bindPrDurSec;
    }

    if (out.evMarked[EV_FC1] && out.evMarked[EV_DROP] && s.evMs[EV_FC1] != EV_NO_TIME
        && s.evMs[EV_DROP] != EV_NO_TIME && s.evMs[EV_DROP] > s.evMs[EV_FC1]) {
        // DR marked: freeze DEV at the time / temperature of DR (does not keep running to END)
        out.devSec       = (s.evMs[EV_DROP] - s.evMs[EV_FC1]) / 1000UL;
        out.devTempDelta = s.evTemp[EV_DROP] - s.evTemp[EV_FC1];
        out.devDtrPct    = (float)(s.evMs[EV_DROP] - s.evMs[EV_FC1]) * 100.0f / (float)s.evMs[EV_DROP];
    } else if (out.evMarked[EV_FC1] && s.evMs[EV_FC1] != EV_NO_TIME && s.batchMs > s.evMs[EV_FC1]) {
        out.devSec       = (s.batchMs - s.evMs[EV_FC1]) / 1000UL;
        out.devTempDelta = s.temp - s.evTemp[EV_FC1];
        out.devDtrPct    = (float)(s.batchMs - s.evMs[EV_FC1]) * 100.0f / (float)s.batchMs;
    } else {
        out.devSec       = 0;
        out.devTempDelta = 0.0f;
        out.devDtrPct    = 0.0f;
    }

    const uint8_t lm = radioLinkMode();
    if (lm == LINK_HIBEAN)       out.linkMode = 0;
    else if (lm == LINK_ARTISAN) out.linkMode = 1;
    else if (lm == LINK_WEB)     out.linkMode = 2;
    else                         out.linkMode = 3;

    out.bleConnected     = g_state.hibeanLinked;
    out.artisanConnected = g_state.artisanLinked;
#if ENABLE_WIFI
    out.wifiApMode   = netApOn();
    String ip = netIP();
    if (ip.length() == 0) ip = "0.0.0.0";
    snprintf(out.ipStr, sizeof(out.ipStr), "%s", ip.c_str());
#else
    out.wifiApMode   = false;
    snprintf(out.ipStr, sizeof(out.ipStr), "0.0.0.0");
#endif
    out.clockSynced = clockFormatHHMM(out.clockStr, sizeof(out.clockStr));
#if ENABLE_WIFI && ENABLE_WEB_PORTAL
    out.webPortalOn  = g_webOn;
    out.webConnected = webClientRecent();
#else
    out.webPortalOn  = false;
    out.webConnected = false;
#endif

    out.overheatC     = g_set.warn;
    out.unitF         = (g_set.unit == UNIT_F);
    out.idleMin       = g_bindIdleMin;
    out.brightnessPct = g_bindBlPct;
    out.volumePct     = g_bindVolPct;
    out.soundMuted    = g_bindMuted;

    out.batPct      = bindBatteryPct();
    out.batCharging = bindBatteryChg();

#if ENABLE_OVERHEAT_ALARM
    // Overheat state machine (RoasterAlarm.h alarmHeatNext): IDLE -> ACTIVE when BT >= threshold;
    // ACTIVE/MUTED return to IDLE only when BT < (threshold - ALARM_HYST_C). Cutting the Heater is the core's job (safetyWatchdog / g_overheatCutC).
    {
        const float warn = (float)g_set.warn;
        const bool  hot  = s.tempOk && (s.temp >= warn);
        const bool  warm = s.tempOk && (s.temp >= warn - (float)ALARM_HYST_C);
        if (g_bindOverheatMuted) {
            g_bindOverheatActive = false;
            if (!warm) g_bindOverheatMuted = false;
        } else if (g_bindOverheatActive) {
            // Keep the screen until the user touches (bindAckOverheat), even after BT has dropped: so an overheat event is not missed
        } else if (hot) {
            g_bindOverheatActive = true;
        }
    }
    out.overheatAlert = g_bindOverheatActive;
    out.overheatCooled = !(s.tempOk && (s.temp >= (float)g_set.warn - (float)ALARM_HYST_C));
#else
    out.overheatAlert = false;
    out.overheatCooled = true;
#endif

    out.signalLost = s.sigLostAlarm;
    if (s.sigLostAlarm) {
        if (g_bindSigLostSince == 0) g_bindSigLostSince = millis();
        out.signalLostSec = (millis() - g_bindSigLostSince) / 1000UL;
    } else {
        g_bindSigLostSince = 0;
        out.signalLostSec  = 0;
    }

}

static void bindStartManual() {
    if (radioLinkMode() == LINK_WEB) netLinkMode(LINK_SOLO, false);
    panelStart();
}

static void bindEndBatch() {
    panelEnd();
    if (batchLive) panelEnd();
}

static void bindSetHeaterPct(int pct) {
    if (pct <= 0) {
        panelHeaterOff();
    } else {
        Snap s;
        roasterSnapshot(s);
        if (s.live && s.vent < 10) {
            panelSetAir(10);
        }
        panelSetHeater(pct);
    }
}
static void bindSetAirPct(int pct)    { panelSetAir(pct); }
static void bindSetPidOn(bool on) {
    if (on) {
        Snap s;
        roasterSnapshot(s);
        if (s.live && s.vent < 10) {
            panelSetAir(10);
        }
    }
    panelSetPID(on);
}
static void bindSetPidSv(float tempC) { panelSetSetpoint(tempC); }
static void bindToggleDrum()          { Snap s; roasterSnapshot(s); panelSetDrum(s.drum == 0); }
static void bindToggleCool()          { Snap s; roasterSnapshot(s); panelSetCool(s.cool == 0); }

static void bindMarkEvent(uint8_t evIdx) {
    if (evIdx == EV_CHARGE && !g_bindPrFrozen && g_bindPrStartMs != 0) {
        g_bindPrDurSec = (bclock() - g_bindPrStartMs) / 1000UL;
        g_bindPrFrozen = true;
    }
    if (evIdx == EV_DROP) {
        panelDrop();
        if (!(evDone & (1u << EV_DROP))) panelDrop();
    } else if (evIdx < EV_COUNT) {
        panelEvent((int)evIdx);
    }
}

static void bindCycleLinkMode() {
    const uint8_t cur = radioLinkMode();
    if (cur == LINK_HIBEAN)        netLinkMode(LINK_ARTISAN, true);
    else if (cur == LINK_ARTISAN)  netLinkMode(LINK_WEB, false);
    else if (cur == LINK_WEB)      netLinkMode(LINK_SOLO, true);
    else                           netLinkMode(LINK_HIBEAN, true);
}

static void bindSetWifiApMode(bool apMode) {
#if ENABLE_WIFI
    if (radioLinkMode() == LINK_HIBEAN) {
        netLinkMode(LINK_SOLO, true);
    }
    netSetAp(apMode);
    if (!apMode && netSavedCount() > 0) netSetEnabled(true);
#else
    (void)apMode;
#endif
}

static void bindSetWebPortal(bool on) {
    netLinkMode(on ? LINK_WEB : LINK_SOLO, false);
}

static void bindReadPidParams(UiPidParams &out) {
#if ENABLE_SETTINGS_PAGE && ENABLE_PID_SETTINGS
    LOCK();
    out.kp = (float)myPIDConfig.getKp();
    out.ki = (float)myPIDConfig.getKi();
    out.kd = (float)myPIDConfig.getKd();
    out.pm = (myPIDConfig.getPMode() == P_ON_M) ? 1 : 0;
    UNLOCK();
#else
    (void)out;
#endif
}

static void bindSavePidParams(const UiPidParams &in) {
#if ENABLE_SETTINGS_PAGE && ENABLE_PID_SETTINGS
    pidCfgApply(in.kp, in.ki, in.kd, in.pm ? 1 : 0);
    pidSettingsSave();
#else
    (void)in;
#endif
}

static void bindResetPidParams(UiPidParams &out) {
#if ENABLE_SETTINGS_PAGE && ENABLE_PID_SETTINGS
    pidSettingsResetDefault();
    bindReadPidParams(out);
#else
    (void)out;
#endif
}

// Touch on the overheat alert (BT already down): close it. The core cuts Heater itself (safetyWatchdog); the UI has no Heater Off button.
static void bindAckOverheat() {
    g_bindOverheatMuted  = true;
    g_bindOverheatActive = false;
}

// OK button on the signal-lost alert: the core closes the alarm; Heater stays blocked until the signal returns
static void bindAckSignalLost() {
    panelSignalLostAck();
}

static void bindSystemReset()    { ESP.restart(); }

// ---- Power off ----
// On battery: pulling the hold pin LOW turns the board off. On USB the board stays powered, so the hold pin does nothing;
// then "off" = deep sleep (backlight, WiFi / BLE, CPU off): press PWR to wake, the device restarts fresh.
#ifndef PWROFF_SETTLE_MS
  #define PWROFF_SETTLE_MS 1000        // wait for the core to send a few "all off" frames to the roaster before power is lost / sleep
#endif
#ifndef PWROFF_SLEEP_WHEN_HOT
  #define PWROFF_SLEEP_WHEN_HOT 0      // 0 = if BT >= DRUM_AUTO_ON_C do NOT deep-sleep (Drum must keep running, and deep sleep stops commands to the roaster)
#endif
static void bindUsbDeepSleep() {
#if defined(BIND_HAS_DEEPSLEEP)
  #if !PWROFF_SLEEP_WHEN_HOT && DRUM_AUTO_ON_C > 0
    double bt;
    LOCK(); bt = temp; UNLOCK();
    if (bt >= (double)DRUM_AUTO_ON_C) {
        DLOGM("PWR", "BT %.0f >= %d C: may con nong, giu chay (khong ngu sau)\n", bt, (int)DRUM_AUTO_ON_C);
        return;
    }
  #endif
    DLOGM("PWR", "cam USB: ngu sau, nhan PWR de danh thuc\n");
    displayBacklight(false);
    ledcDetach(TFT_BLK);                                 // PWM stops in sleep: keep the backlight pin off
    pinMode(TFT_BLK, OUTPUT);
    digitalWrite(TFT_BLK, LOW);
    gpio_hold_en((gpio_num_t)TFT_BLK);
  #if ROASTER_TX_PIN >= 0
    pinMode(ROASTER_TX_PIN, OUTPUT);                     // hold the roaster command line at idle (HIGH), not floating
    digitalWrite(ROASTER_TX_PIN, HIGH);
    gpio_hold_en((gpio_num_t)ROASTER_TX_PIN);
  #endif
    gpio_deep_sleep_hold_en();
    pinMode(BTN_PWR_PIN, INPUT_PULLUP);
    const uint32_t t0 = millis();
    while (digitalRead(BTN_PWR_PIN) == LOW && (uint32_t)(millis() - t0) < 3000UL) delay(10);   // wait for PWR release, otherwise it wakes right after sleeping
    delay(100);
    rtc_gpio_pullup_en((gpio_num_t)BTN_PWR_PIN);
    rtc_gpio_pulldown_dis((gpio_num_t)BTN_PWR_PIN);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_PWR_PIN, 0);   // PWR pulled LOW = wake
    esp_deep_sleep_start();                              // does not return; wake = restart
#endif
}

// Wake from deep sleep: pins held before sleep stay LOCKED until gpio_hold_dis(). If not released, the backlight
// (TFT_BLK) stays LOW -> black screen although the device runs again (Welcome chirp still heard), and the roaster command pin stays HIGH.
// Call at the start of uiBindCoreV3(), BEFORE ui_task initializes the backlight PWM. On a cold boot these calls are harmless.
static void bindReleaseSleepHold() {
#if defined(BIND_HAS_DEEPSLEEP)
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis((gpio_num_t)TFT_BLK);
  #if ROASTER_TX_PIN >= 0
    gpio_hold_dis((gpio_num_t)ROASTER_TX_PIN);
  #endif
#endif
}

static void bindSystemPowerOff() {
#if CUE_ENABLE
    g_alarm_sound = ALARM_SND_SLEEP;                     // sleep chirp (real power off and deep sleep both), played alongside the all-off commands
#endif
    roasterShutdown();                                   // all off to 0 (Drum kept on only if BT is still hot)
    {
        uint32_t waitMs = (uint32_t)PWROFF_SETTLE_MS;
#if CUE_ENABLE
        const uint32_t cueMs = (uint32_t)CUE_SLEEP_MS + 300UL;   // wait for the chirp to finish (+ the last silent block) before power goes
        if (cueMs > waitMs) waitMs = cueMs;
#endif
        delay(waitMs);
    }
#if ENABLE_BATTERY
    batteryPowerOff();                                   // on battery: board off, does not return. If it returns, USB is plugged in
#endif
    bindUsbDeepSleep();
}

static void bindFinishBootToSolo() {
    if (g_bindBootSoloDone) return;
    g_bindBootSoloDone = true;
#if ENABLE_WIFI && ENABLE_WEB_PORTAL
    // Default after the splash page: enter Web Portal to keep WiFi up for NTP / browser time sync
    // (pressing START switches to LINK_SOLO, WiFi/Web off)
    netLinkMode(LINK_WEB, false);
#else
    netLinkMode(LINK_SOLO, false);
#endif
}

static inline void uiBindCoreV3() {
    bindReleaseSleepHold();   // wake from deep sleep: release the backlight / roaster command pin holds
    bindLoadIdleMin();
    bindLoadBrightness();
    bindLoadSound();
#if ENABLE_WIFI
    // During the splash screen (12 s), pre-connect the saved WiFi to sync NTP time
    if (netSavedCount() > 0) {
        netSetAp(false);
        netSetEnabled(true);
    }
#endif
    g_uiOps.readSnapshot     = bindReadSnapshot;
    g_uiOps.startManual      = bindStartManual;
    g_uiOps.endBatch         = bindEndBatch;
    g_uiOps.setHeaterPct     = bindSetHeaterPct;
    g_uiOps.setAirPct        = bindSetAirPct;
    g_uiOps.setPidOn         = bindSetPidOn;
    g_uiOps.setPidSv         = bindSetPidSv;
    g_uiOps.toggleDrum       = bindToggleDrum;
    g_uiOps.toggleCool       = bindToggleCool;
    g_uiOps.markEvent        = bindMarkEvent;
    g_uiOps.cycleLinkMode    = bindCycleLinkMode;
    g_uiOps.setWifiApMode    = bindSetWifiApMode;
    g_uiOps.setWebPortal     = bindSetWebPortal;
    g_uiOps.readPidParams    = bindReadPidParams;
    g_uiOps.savePidParams    = bindSavePidParams;
    g_uiOps.resetPidParams   = bindResetPidParams;
    g_uiOps.setOverheatC     = bindSetOverheatC;
    g_uiOps.setTempUnitF     = bindSetTempUnitF;
    g_uiOps.setIdleMin       = bindSetIdleMin;
    g_uiOps.setBrightnessPct = bindSetBrightness;
    g_uiOps.setVolumePct     = bindSetVolume;
    g_uiOps.setSoundMuted    = bindSetMuted;
    g_uiOps.ackOverheat      = bindAckOverheat;
    g_uiOps.systemReset      = bindSystemReset;
    g_uiOps.systemPowerOff   = bindSystemPowerOff;
    g_uiOps.finishBootToSolo = bindFinishBootToSolo;
    g_uiOps.ackSignalLost    = bindAckSignalLost;

#if ENABLE_WIFI && ENABLE_WEB_PORTAL
    g_webLangCb = []() -> uint8_t {
        const UiLang l = uiLangGet();
        if (l == UILANG_VI) return WL_VI;
        if (l == UILANG_CN) return WL_CN;
        return WL_EN;
    };
#endif
}
