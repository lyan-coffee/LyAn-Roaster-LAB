/*
 * lib/RoasterUI.h - LVGL 240x240 Roaster Cockpit v7 (core 0 task + Display/Touch/Buttons + Idle Logo + 100 ms sync_cb) (tier 8)
 */
#pragma once
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include "../RoasterSetting.h"
#include "UIConfig.h"
#include "UILang.h"
#include "RoasterSync.h"
#include "RoasterState.h"
#include "RoasterTick.h"
#include "RoasterDisplay.h"
#if ENABLE_TOUCH
  #include "RoasterTouch.h"
#endif
#if ENABLE_BUTTONS
  #include "RoasterButtons.h"
#endif
#include "RoasterUIBridge.h"
#include "RoasterUIWidgets.h"
#include "RoasterUINav.h"
#include "RoasterUIWelcome.h"
#include "RoasterUIIdle.h"
#include "RoasterUIAlert.h"
#include "RoasterUIPower.h"
#include "RoasterUIMain.h"
#include "RoasterUIControl.h"
#include "RoasterUIEvent.h"

#define HAS_MODULE_UI       1
#define HAS_MODULE_UI_FRAME 1

static TaskHandle_t g_uiTask             = nullptr;
static lv_display_t *g_disp              = nullptr;
static uint8_t      g_uiBufLines         = 0;
static lv_obj_t    *g_uiActiveScr        = nullptr;
static bool         g_uiSignalDismissed  = false;
static uint32_t     g_uiLastActivityMs   = 0;
static bool         g_uiIdleSwallowTouch = false;

static inline void uiMarkActivity() {
    g_uiLastActivityMs = millis();
}

// ---- Screen off with PWR (HOME page, IDLE state, not roasting) ----
// Cricket chirps (screen off / on, enter IDLE): length, volume and frequency are set in lib/RoasterCue.h (CUE_ENABLE, CUE_*_MS, CUE_*_VOL)
static bool     g_uiScreenOff     = false;
static bool     g_uiSleepPending  = false;   // PWR pressed, waiting for the sleep sound to finish before turning the backlight off
static uint32_t g_uiSleepAt       = 0;

static inline void uiScreenSleep() {
    if (g_uiScreenOff || g_uiSleepPending) return;
    g_uiSleepPending = true;
#if CUE_ENABLE
    g_alarm_sound = ALARM_SND_SLEEP;
    g_uiSleepAt = millis() + (uint32_t)CUE_SLEEP_MS;   // backlight off after the chirp finishes
#else
    g_uiSleepAt = millis();
#endif
}

// Wake the screen. Returns true if it was off / about to turn off (so the first key / touch is swallowed).
static inline bool uiScreenWake(bool chime) {
    if (!g_uiScreenOff && !g_uiSleepPending) return false;
    const bool wasOff = g_uiScreenOff;
    g_uiSleepPending = false;
    g_uiScreenOff = false;
    uiMarkActivity();
    if (wasOff) {
        displayBacklight(true);
        uiNavRequestRebuild();               // rebuild from scratch (clock, battery...)
    }
#if CUE_ENABLE
    if (chime) g_alarm_sound = ALARM_SND_WAKE;
    else if (g_alarm_sound == ALARM_SND_SLEEP) g_alarm_sound = ALARM_SND_OFF;
#else
    (void)chime;
#endif
    return true;
}

#ifndef UI_TOUCH_SCROLL_LIMIT
  #define UI_TOUCH_SCROLL_LIMIT 15   // px: a light finger roll on touch is not treated by LVGL as a drag/swipe (LVGL default 10 px)
#endif

// Leave the IDLE Logo page: close the overlay + request a rebuild of the current page (deferred to sync_cb per rule 6.2)
// so every cached display value (HH:MM clock, IP, battery...) is redrawn from scratch.
static inline void uiIdleWake() {
    if (!uiIdleIsOpen()) return;
    uiIdleClose();
    uiNavRequestRebuild();
}

static void uiGestureCb(lv_event_t *e) {
    (void)e;
    if (uiAlertBlocksInput() || uiPowerIsOpen() || uiIdleIsOpen()) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) {
        if (uiCtrlOverlayOpen() || uiEventConfirmOpen()) return;
        uiNavSwipe(g_uiSnap.batchLive);
    }
}

static inline void uiRebuildNow() {
    g_uiRebuildReq = false;
    g_uiSplash = (g_uiPage == UI_PAGE_WELCOME);

    lv_obj_t *scr = lv_obj_create(nullptr);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, UI_SCR_W, UI_SCR_H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_HEX_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, uiGestureCb, LV_EVENT_GESTURE, nullptr);

    switch (g_uiPage) {
        case UI_PAGE_WELCOME: uiBuildWelcome(scr); break;
        case UI_PAGE_HOME:    uiBuildHome(scr);    break;
        case UI_PAGE_CONTROL: uiBuildControl(scr); break;
        case UI_PAGE_EVENT:   uiBuildEvent(scr);   break;
        case UI_PAGE_SETTING: uiBuildSetting(scr); break;
        case UI_PAGE_WIFI:    uiBuildWifi(scr);    break;
        case UI_PAGE_PID:     uiBuildPid(scr);     break;
        case UI_PAGE_SYSTEM:  uiBuildSystem(scr);  break;
    }

    lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    g_uiActiveScr = scr;
}

// Touch on the signal-lost alert: tell the core panelSignalLostAck(); the alert closes on the next tick
static void uiAlertDismissSigCb() {
    uiAlertSoundMute();
    g_uiSignalDismissed = true;
    if (g_uiOps.ackSignalLost) g_uiOps.ackSignalLost();
}
// Touch on the overheat alert: always mute the speaker. If BT has dropped below (threshold - ALARM_HYST_C) tell the core to close the alert;
// if still hot, the alert STAYS (the user still sees the warning; touch again after cooling to close).
static void uiAlertDismissHeatCb() {
    uiAlertSoundMute();
    if (g_uiSnap.overheatCooled && g_uiOps.ackOverheat) g_uiOps.ackOverheat();
}

static inline void uiOnBtnBootShort() {
    if (uiAlertBlocksInput()) return;
    if (uiPowerIsOpen()) { uiPowerClose(); return; }
    if (g_uiPage == UI_PAGE_CONTROL && uiCtrlOverlayOpen()) { uiCtrlAdjustOverlay(-1, false); return; }
    if (g_uiPage == UI_PAGE_PID) { uiPidHardwareBtn(-1, false); return; }
    if (g_uiPage == UI_PAGE_SYSTEM) { uiSystemHardwareBtn(-1, false); return; }
    uiNavSwipe(g_uiSnap.batchLive);
}

static inline void uiOnBtnPlusShort() {
    if (uiAlertBlocksInput()) return;
    if (uiPowerIsOpen()) { uiPowerClose(); return; }
    if (g_uiPage == UI_PAGE_CONTROL && uiCtrlOverlayOpen()) { uiCtrlAdjustOverlay(+1, false); return; }
    if (g_uiPage == UI_PAGE_PID) { uiPidHardwareBtn(+1, false); return; }
    if (g_uiPage == UI_PAGE_SYSTEM) { uiSystemHardwareBtn(+1, false); return; }
    uiNavSwipe(g_uiSnap.batchLive);
}

static inline void uiOnBtnPwrShort() {
    if (uiAlertBlocksInput()) return;
    if (uiPowerIsOpen()) { uiPowerClose(); return; }
    if (g_uiPage == UI_PAGE_CONTROL && uiCtrlOverlayOpen()) { uiCtrlCloseOverlay(); return; }
    if (g_uiPage == UI_PAGE_EVENT && uiEventConfirmOpen()) { uiEventCloseConfirm(); return; }
    if (g_uiPage == UI_PAGE_PID) { uiPidHardwareBtn(0, true); return; }
    if (g_uiPage == UI_PAGE_SYSTEM) { uiSystemHardwareBtn(0, true); return; }
    if (g_uiPage == UI_PAGE_HOME && !g_uiSnap.batchLive) uiScreenSleep();   // HOME + IDLE: short PWR = screen off
}

static inline void uiOnBtnPwrLong5s() {
    if (uiAlertBlocksInput()) return;
    uiPowerOpen();
}

static inline void uiDrainButtons() {
#if ENABLE_BUTTONS
    BtnEvent ev;
    while (buttonPop(ev)) {
        uiMarkActivity();
        if (uiScreenWake(true)) continue;   // screen off: any key only wakes it (key swallowed)
        // On the IDLE Logo page: any key closes it and returns to the previous screen (the first key event is swallowed)
        if (uiIdleIsOpen()) {
            uiIdleWake();
            continue;
        }
        if (ev.id == BTN_BOOT && ev.type == BTN_SHORT) uiOnBtnBootShort();
        else if (ev.id == BTN_PLUS && ev.type == BTN_SHORT) uiOnBtnPlusShort();
        else if (ev.id == BTN_PWR && ev.type == BTN_SHORT) uiOnBtnPwrShort();
        else if (ev.id == BTN_PWR && ev.type == BTN_LONG) uiOnBtnPwrLong5s();
    }
#endif
}

static inline void uiSyncTick() {
    uiBridgeRefresh();

#if CUE_ENABLE && CUE_IDLE_REPEAT_MS > 0
    // IDLE screen (Logo page): after the entry chirp, chirp again every CUE_IDLE_REPEAT_MS (5 min) until IDLE exits.
    {
        static bool     wasIdle = false;
        static uint32_t nextAt  = 0;
        static bool     altHigh = true;   // repeat chirps alternate two voices: IDLE (2400 Hz) -> WAKE (3200 Hz) -> IDLE -> ...
        const bool idleNow = uiIdleIsOpen() && !g_uiScreenOff && !g_uiSleepPending;
        if (idleNow && !wasIdle) { nextAt = millis() + (uint32_t)CUE_IDLE_REPEAT_MS; altHigh = true; }
        else if (idleNow && static_cast<int32_t>(millis() - nextAt) >= 0) {
            nextAt = millis() + (uint32_t)CUE_IDLE_REPEAT_MS;
            if (g_alarm_sound == ALARM_SND_OFF) {
                g_alarm_sound = altHigh ? ALARM_SND_WAKE : ALARM_SND_IDLE;
                altHigh = !altHigh;
            }
        }
        wasIdle = idleNow;
    }
#endif
#if CUE_ENABLE
    // Boot chirp: once Welcome has shown for CUE_BOOT_DELAY_MS (3 s), chirp CUE_BOOT_COUNT times (chirp length + CUE_BOOT_GAP_MS apart)
    if (g_uiPage == UI_PAGE_WELCOME && g_uiWelcome.cues < CUE_BOOT_COUNT) {
        const uint32_t el  = millis() - g_uiWelcome.startMs;
        const uint32_t due = (uint32_t)CUE_BOOT_DELAY_MS + (uint32_t)g_uiWelcome.cues * ((uint32_t)CUE_WELCOME_MS + (uint32_t)CUE_BOOT_GAP_MS);
        if (el >= due) { g_alarm_sound = ALARM_SND_WELCOME; g_uiWelcome.cues++; }
    }
#endif

    // Screen off / about to turn off: a safety event turns it back on (no wake sound); otherwise turn the backlight off when due
    if (g_uiScreenOff || g_uiSleepPending) {
        if (g_uiSnap.overheatAlert || (g_uiSnap.signalLost && !g_uiSignalDismissed) || g_uiSnap.batchLive) {
            uiScreenWake(false);
        } else if (g_uiSleepPending && static_cast<int32_t>(millis() - g_uiSleepAt) >= 0) {
            g_uiSleepPending = false;
            g_uiScreenOff = true;
            displayBacklight(false);
        }
    }

    // Overheat takes priority over Signal lost; no alert left -> remove the screen
    if (!g_uiSnap.signalLost) g_uiSignalDismissed = false;
    const bool wantSig = g_uiSnap.signalLost && !g_uiSignalDismissed;
    // Sound: overheat (skipped if Heater Off / Dismiss pressed) > signal lost; a different alert type makes the speaker sound again
    uiAlertSoundUpdate((g_uiSnap.overheatAlert && !g_uiSnap.overheatCooled) ? ALARM_SND_HEAT : (wantSig ? ALARM_SND_LOST : ALARM_SND_OFF));
    if (g_uiSnap.overheatAlert) {
        uiIdleWake();
        uiAlertShowOverheat(g_uiSnap.bt, uiAlertDismissHeatCb, g_uiSnap.overheatC);
        return;
    }
    if (wantSig) {
        uiIdleWake();
        uiAlertShowSignalLost(g_uiSnap.signalLostSec, g_uiSnap.bt, uiAlertDismissSigCb);
    } else if (uiAlertIsActive()) {
        uiAlertHide();
    }

    if (g_uiSnap.batchLive) {
        uiIdleWake();
        uiMarkActivity();
        if (g_uiPage == UI_PAGE_SETTING || g_uiPage == UI_PAGE_WIFI ||
            g_uiPage == UI_PAGE_PID || g_uiPage == UI_PAGE_SYSTEM) {
            if (g_uiSnap.webPortalOn && g_uiOps.setWebPortal) g_uiOps.setWebPortal(false);
            uiNavGo(UI_PAGE_CONTROL);
        }
    }

    // After the splash page: the core decides when to turn WiFi off (holds up to 30 s for NTP time), called every tick until done
    if (g_uiPage != UI_PAGE_WELCOME && g_uiOps.finishBootToSolo) g_uiOps.finishBootToSolo();

    if (g_uiRebuildReq) {
        uiRebuildNow();
    }

    switch (g_uiPage) {
        case UI_PAGE_WELCOME: uiSyncWelcome(); break;
        case UI_PAGE_HOME:    uiSyncHome();    break;
        case UI_PAGE_CONTROL: uiSyncControl(); break;
        case UI_PAGE_EVENT:   uiSyncEvent();   break;
        default: uiSyncMainBattery(); break;
    }

    // Check the timer that opens the IDLE Logo page (keeps the backlight on, screen not off)
    if (!g_uiSnap.batchLive &&
        g_uiSnap.idleMin > 0 &&
        g_uiPage != UI_PAGE_WELCOME &&
        !uiAlertIsActive() &&
        !uiPowerIsOpen() &&
        !g_uiScreenOff && !g_uiSleepPending &&
        !uiIdleIsOpen()) {
        const uint32_t idleMs = static_cast<uint32_t>(g_uiSnap.idleMin) * 60000UL;
        if ((millis() - g_uiLastActivityMs) >= idleMs) {
#if CUE_ENABLE
            g_alarm_sound = ALARM_SND_IDLE;      // chirp on entering IDLE
#endif
            uiIdleOpen();
        }
    }
}

static void ui_sync_cb(lv_timer_t *) {
    tickRun();
    uiSyncTick();
}

static uint32_t ui_lv_tick() { return millis(); }

static void ui_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    displayFlush(area->x1, area->y1, w, h, (const uint16_t *)px_map);
    lv_display_flush_ready(disp);
}

#if ENABLE_TOUCH
static void ui_touch_read(lv_indev_t *, lv_indev_data_t *data) {
    uint16_t x = 0, y = 0;
    bool down = touchRead(x, y);
    if (uiAlertBlocksInput()) down = false;

    if (down) {
        uiMarkActivity();
        if (uiScreenWake(true)) {            // screen off: a touch only wakes it, the first touch is swallowed
            g_uiIdleSwallowTouch = true;
            down = false;
        } else
        // On the IDLE Logo page: touch wakes at once and the first touch is swallowed until release
        if (uiIdleIsOpen()) {
            uiIdleWake();
            g_uiIdleSwallowTouch = true;
            down = false;
        } else if (g_uiIdleSwallowTouch) {
            down = false;
        }
    } else {
        g_uiIdleSwallowTouch = false;
    }

    data->state   = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = x;
    data->point.y = y;
}
#endif

static void ui_task(void *) {
    DLOGM("UI", "task start (Roaster Cockpit v6)\n");
    if (!displayBegin()) {
        for (;;) {
            displayBacklight(true);  vTaskDelay(pdMS_TO_TICKS(250));
            displayBacklight(false); vTaskDelay(pdMS_TO_TICKS(250));
        }
    }

    lv_init();
    lv_tick_set_cb(ui_lv_tick);

    uint8_t *buf = nullptr;
    uint32_t buf_bytes = 0;
    for (int lines = UI_BUF_LINES; lines >= 8; lines -= 8) {
        buf_bytes = (uint32_t)SCREEN_W * lines * sizeof(uint16_t);
        buf = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (buf) { g_uiBufLines = (uint8_t)lines; break; }
    }
    if (!buf) {
        for (;;) {
            displayBacklight(true);  vTaskDelay(pdMS_TO_TICKS(100));
            displayBacklight(false); vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    g_disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_flush_cb(g_disp, ui_disp_flush);
    lv_display_set_buffers(g_disp, buf, nullptr, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

#if ENABLE_TOUCH
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, ui_touch_read);
    lv_timer_set_period(lv_indev_get_read_timer(indev), UI_TOUCH_READ_MS);
    lv_indev_set_scroll_limit(indev, UI_TOUCH_SCROLL_LIMIT);
#endif

    alarmSoundStart();   // the ES8311 codec shares I2C with touch, so it is initialized here in this task (alert speaker)
    uiLangInit();
    uiMarkActivity();
    g_uiPage = UI_PAGE_WELCOME;
    g_uiSplash = true;
    uiRebuildNow();

    lv_timer_create(ui_sync_cb, TICK_MS, nullptr);
    lv_timer_handler();
    displayBacklight(true);
    g_uiReady = true;
    DLOGM("UI", "ready, internal heap free %u KB\n",
          (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024U));

    for (;;) {
        uiDrainButtons();
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void startUI() {
    xTaskCreatePinnedToCore(ui_task, "ui", UI_TASK_STACK, nullptr, 3, &g_uiTask, 0);
}
