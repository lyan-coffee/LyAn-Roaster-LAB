#pragma once
#include "RoasterUIWidgets.h"
#include "RoasterUINav.h"
#include "RoasterUIBridge.h"

#define UI_CTRL_ICON_PX 38   // Icons Heater/Air/Drum/Cool/popup: one size, 38 px (native 38, see RoasterIcons.h)
#define UI_CTRL_BTN_PAD 12   // Heater/Air button 62 high: icon top margin = bottom margin = 12 px
#define UI_CTRL_BTN_PADX 10  // icon left margin = value right margin = 10 px (2 px further out than 12)

// Solid single-color button (END red, Heater popup orange, Air popup blue): solid fill, black text; darkens 20% when pressed
static inline void uiCtrlSolidBtn(lv_obj_t *btn, uint32_t hex) {
    const lv_style_selector_t sel[2] = { LV_PART_MAIN, (lv_style_selector_t)LV_PART_MAIN | LV_STATE_PRESSED };
    for (int i = 0; i < 2; i++) {
        const lv_color_t c = lv_color_hex(i == 0 ? hex : UI_MIX_HEX(hex, 80));
        lv_obj_set_style_bg_color(btn, c, sel[i]);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, sel[i]);
        lv_obj_set_style_border_color(btn, c, sel[i]);
    }
}
static inline void uiCtrlEndStyle(lv_obj_t *btn) { uiCtrlSolidBtn(btn, UI_HEX_RED); }


enum UiCtrlOverlay : uint8_t {
    UI_OVL_NONE = 0,
    UI_OVL_HEATER,
    UI_OVL_AIR
};

struct UiControlState {
    UiShell        shell            = {};
    lv_obj_t      *badgeSimLive     = nullptr;
    lv_obj_t      *lblSimLive       = nullptr;
    lv_obj_t      *lblBt            = nullptr;   // BT integer part (large)
    lv_obj_t      *lblBtDec         = nullptr;   // BT decimal part (~7/10)
    lv_obj_t      *lblBtUnit        = nullptr;
    lv_obj_t      *lblRor           = nullptr;   // RoR integer part
    lv_obj_t      *lblRorDec        = nullptr;   // RoR decimal part (~7/10)

    UiToneBtn      btnHeater        = {};
    lv_obj_t      *imgHeater        = nullptr;
    lv_obj_t      *lblHeater        = nullptr;
    lv_obj_t      *lblHeaterUnit    = nullptr;

    UiToneBtn      btnAir           = {};
    lv_obj_t      *imgAir           = nullptr;
    lv_obj_t      *lblAir           = nullptr;

    UiToneBtn      btnDrum          = {};
    lv_obj_t      *imgDrum          = nullptr;
    UiToneBtn      btnCool          = {};
    lv_obj_t      *imgCool          = nullptr;
    UiToneBtn      btnEnd           = {};
    lv_obj_t      *imgEnd           = nullptr;

    // 5 s confirmation popup for the END button
    UiConfirmPopup confirmEnd       = {};

    // Heater / Air popup (stable touch area, blocks gesture bubbling, no double tap)
    UiCtrlOverlay  ovlMode          = UI_OVL_NONE;
    lv_obj_t      *ovlBackdrop      = nullptr;
    lv_obj_t      *ovlPanel         = nullptr;
    lv_obj_t      *ovlVal           = nullptr;
    lv_obj_t      *ovlValUnit       = nullptr;
    lv_obj_t      *ovlSwPid         = nullptr;
    lv_obj_t      *ovlIco           = nullptr;   // icon in the popup (under the PID text)
    lv_obj_t      *ovlPidTag        = nullptr;   // PID text over the flame icon in the popup
    lv_obj_t      *tagHeater        = nullptr;   // PID text over the flame icon on the Heater button
    uint32_t       ovlGuardUntil    = 0;
    uint32_t       ovlHoldSyncUntil = 0;
    uint32_t       ovlArmAt         = 0;   // OFF / - / + / PID accept touch only after this time (anti-bounce right after the popup opens)

    // Dirty-rect caches
    int            lastSim          = -1;
    int            lastBt10         = -9999;
    int            lastRor10        = -9999;
    uint32_t       lastSec          = 0xFFFFFFFF;
    char           lastClock[8]     = "";
    int            lastHtr          = -1;
    int            lastPidOn        = -1;
    int            lastPidSv        = -1;
    int            lastAir          = -1;
    int            lastDrum         = -1;
    int            lastCool         = -1;
    int            lastLive         = -1;
    int            lastUnitF        = -1;
};

static UiControlState g_uiCtrl;

// Mixed-size text on one baseline: the label bottom shifts by the font's base_line difference from the main font
static inline int32_t uiCtrlBaseY(const lv_font_t *f, const lv_font_t *ref) {
    return -4 + (int32_t)f->base_line - (int32_t)ref->base_line;   // -4: RoR / BT text bottom is 4 px above the bottom edge
}
static inline int32_t uiCtrlTxtW(lv_obj_t *lbl, const lv_font_t *f) {
    const char *t = lv_label_get_text(lbl);
    return lv_text_get_width(t, (uint32_t)strlen(t), f, 0);
}
// BT: [large integer][.d ~7/10][°C right after], left aligned
static void uiCtrlBtLayout() {
    uiBtRowLayout(g_uiCtrl.lblBt, g_uiCtrl.lblBtDec, g_uiCtrl.lblBtUnit, UI_FONT_C_BT, UI_FONT_C_BTD, UI_FONT_C_UNIT, -4);
}
// Cream PID text over the flame icon when PID is on (replaces the PID label); the icon is dimmed so the text is readable
static inline lv_obj_t* uiCtrlPidTagCreate(lv_obj_t *parent, lv_obj_t *icon) {
    lv_obj_t *t = lv_label_create(parent);
    lv_label_set_text(t, "PID");
    lv_obj_set_style_text_font(t, UI_FONT_C_UNIT, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, lv_color_hex(UI_HEX_TEXT), LV_PART_MAIN);
    lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align_to(t, icon, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(t, LV_OBJ_FLAG_HIDDEN);
    return t;
}
static inline void uiCtrlPidTagSet(lv_obj_t *tag, lv_obj_t *icon, bool pidOn) {
    if (!tag || !icon) return;
    if (pidOn) { lv_obj_remove_flag(tag, LV_OBJ_FLAG_HIDDEN); lv_obj_set_style_image_opa(icon, LV_OPA_50, LV_PART_MAIN); }
    else       { lv_obj_add_flag(tag, LV_OBJ_FLAG_HIDDEN); }   // icon returns to its own brightness via uiIconV3SetOn
}
// RoR: [integer][.d ~7/10], right aligned
static void uiCtrlRorLayout() {
    if (!g_uiCtrl.lblRor || !g_uiCtrl.lblRorDec) return;
    const int32_t wd = uiCtrlTxtW(g_uiCtrl.lblRorDec, UI_FONT_C_VALD);
    lv_obj_align(g_uiCtrl.lblRorDec, LV_ALIGN_BOTTOM_RIGHT, -2,       uiCtrlBaseY(UI_FONT_C_VALD, UI_FONT_C_VAL));
    lv_obj_align(g_uiCtrl.lblRor,    LV_ALIGN_BOTTOM_RIGHT, -2 - wd,  uiCtrlBaseY(UI_FONT_C_VAL,  UI_FONT_C_VAL));
}

static inline bool uiCtrlOverlayOpen() {
    return (g_uiCtrl.ovlMode != UI_OVL_NONE) || uiConfirmIsOpen(g_uiCtrl.confirmEnd);
}

static inline void uiCtrlCloseOverlay() {
    if (uiConfirmIsOpen(g_uiCtrl.confirmEnd)) {
        uiConfirmClose(g_uiCtrl.confirmEnd);
    }
    if (g_uiCtrl.ovlBackdrop) {
        lv_obj_delete(g_uiCtrl.ovlBackdrop);
        g_uiCtrl.ovlBackdrop      = nullptr;
        g_uiCtrl.ovlPanel         = nullptr;
        g_uiCtrl.ovlVal           = nullptr;
        g_uiCtrl.ovlValUnit       = nullptr;
        g_uiCtrl.ovlSwPid         = nullptr;
        g_uiCtrl.ovlIco           = nullptr;
        g_uiCtrl.ovlPidTag        = nullptr;
    }
    g_uiCtrl.ovlMode          = UI_OVL_NONE;
    g_uiCtrl.ovlGuardUntil    = 0;
    g_uiCtrl.ovlHoldSyncUntil = 0;
    g_uiCtrl.ovlArmAt         = 0;
}

#ifndef UI_OVL_ARM_MS
  #define UI_OVL_ARM_MS 350UL   // ms: lock the popup buttons right after opening (CST816 often reports release/touch again at the same spot)
#endif
static inline bool uiCtrlOvlArmed() {
    return static_cast<int32_t>(millis() - g_uiCtrl.ovlArmAt) >= 0;
}

static inline void uiCtrlTouchPopupGuard() {
    g_uiCtrl.ovlGuardUntil    = millis() + 320UL;
    g_uiCtrl.ovlHoldSyncUntil = millis() + 450UL;
}

static inline void uiCtrlRefreshOvlVal() {
    if (!g_uiCtrl.ovlVal || !g_uiCtrl.ovlValUnit) return;
    if (g_uiCtrl.ovlMode == UI_OVL_HEATER) {
        // PID ON (SV °C) and PID OFF (%) both use the same Heater orange; the small °C/% matches the outside
        if (g_uiSnap.pidOn) {
            int sv = static_cast<int>(lroundf(uiTempDisp(g_uiSnap.pidSv)));
            lv_label_set_text_fmt(g_uiCtrl.ovlVal, "%d", sv);
            lv_label_set_text(g_uiCtrl.ovlValUnit, uiTempUnit());
        } else {
            lv_label_set_text_fmt(g_uiCtrl.ovlVal, "%d", g_uiSnap.heaterPct);
            lv_label_set_text(g_uiCtrl.ovlValUnit, "%");
        }
        lv_obj_set_style_text_color(g_uiCtrl.ovlVal, lv_color_hex(UI_HEX_ORANGE), LV_PART_MAIN);
        lv_obj_set_style_text_color(g_uiCtrl.ovlValUnit, lv_color_hex(UI_HEX_ORANGE), LV_PART_MAIN);
        lv_obj_align_to(g_uiCtrl.ovlValUnit, g_uiCtrl.ovlVal, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -4);
        if (g_uiCtrl.ovlIco) {
            uiIconV3SetOn(g_uiCtrl.ovlIco, true, UI_HEX_RED_OFF);   // restore the icon to full brightness before overlaying PID
            uiCtrlPidTagSet(g_uiCtrl.ovlPidTag, g_uiCtrl.ovlIco, g_uiSnap.pidOn);
        }
    } else if (g_uiCtrl.ovlMode == UI_OVL_AIR) {
        lv_label_set_text_fmt(g_uiCtrl.ovlVal, "%d", g_uiSnap.airPct);
        lv_label_set_text(g_uiCtrl.ovlValUnit, "%");
        lv_obj_set_style_text_color(g_uiCtrl.ovlVal, lv_color_hex(UI_HEX_CYAN), LV_PART_MAIN);
        lv_obj_set_style_text_color(g_uiCtrl.ovlValUnit, lv_color_hex(UI_HEX_CYAN), LV_PART_MAIN);
        lv_obj_align_to(g_uiCtrl.ovlValUnit, g_uiCtrl.ovlVal, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -4);
    }
}

static inline void uiCtrlAdjustOverlay(int deltaStep, bool turnOff = false) {
    uiCtrlTouchPopupGuard();
    if (g_uiCtrl.ovlMode == UI_OVL_HEATER) {
        if (turnOff) {
            g_uiSnap.pidOn = false;
            g_uiSnap.heaterPct = 0;
            if (g_uiOps.setHeaterPct) g_uiOps.setHeaterPct(0);
            if (g_uiCtrl.ovlSwPid) lv_obj_remove_state(g_uiCtrl.ovlSwPid, LV_STATE_CHECKED);
            uiCtrlRefreshOvlVal();
        } else if (g_uiSnap.pidOn) {
            // PID ON: adjust Set temperature SV (step 5°C, 0..300°C)
            if (g_uiSnap.batchLive && g_uiSnap.airPct < 10) g_uiSnap.airPct = 10;
            float baseSv = roundf(g_uiSnap.pidSv / 5.0f) * 5.0f;
            float nextSv = constrain(baseSv + deltaStep * 5.0f, 0.0f, 300.0f);
            g_uiSnap.pidSv = nextSv;
            if (g_uiOps.setPidSv) g_uiOps.setPidSv(nextSv);
            uiCtrlRefreshOvlVal();
        } else {
            // PID OFF: adjust Heater power % (step 5%, 0..100%)
            int next = constrain(g_uiSnap.heaterPct + deltaStep * UI_HEATER_STEP, 0, 100);
            if (next > 0 && g_uiSnap.batchLive && g_uiSnap.airPct < 10) {
                g_uiSnap.airPct = 10; // set Air = 10% at once to sync the UI
            }
            g_uiSnap.heaterPct = next;
            if (g_uiOps.setHeaterPct) g_uiOps.setHeaterPct(next);
            uiCtrlRefreshOvlVal();
        }
    } else if (g_uiCtrl.ovlMode == UI_OVL_AIR) {
        int next = turnOff ? 0 : constrain(g_uiSnap.airPct + deltaStep * UI_AIR_STEP, 0, 100);
        g_uiSnap.airPct = next;
        if (g_uiOps.setAirPct) g_uiOps.setAirPct(next);
        uiCtrlRefreshOvlVal();
    }
}

static void uiOvlBgClickCb(lv_event_t *e) {
    if (lv_event_get_target(e) != g_uiCtrl.ovlBackdrop) return;
    if (static_cast<int32_t>(millis() - g_uiCtrl.ovlGuardUntil) < 0) return;
    lv_indev_t *indev = lv_indev_active();
    if (indev && g_uiCtrl.ovlPanel) {
        lv_point_t pt{};
        lv_indev_get_point(indev, &pt);
        int32_t panelTop = lv_obj_get_y(g_uiCtrl.ovlPanel);
        // close only on a clear touch in the area above the popup frame (16 px from the top edge)
        if (pt.y < panelTop - 16) {
            uiCtrlCloseOverlay();
        }
    }
}

static void uiOvlMinusCb(lv_event_t *e) { (void)e; if (!uiCtrlOvlArmed()) return; uiCtrlAdjustOverlay(-1, false); }
static void uiOvlPlusCb(lv_event_t *e)  { (void)e; if (!uiCtrlOvlArmed()) return; uiCtrlAdjustOverlay(+1, false); }
static void uiOvlOffCb(lv_event_t *e)   { (void)e; if (!uiCtrlOvlArmed()) return; uiCtrlAdjustOverlay(0, true); }
static void uiOvlPidCb(lv_event_t *e) {
    lv_obj_t *sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!uiCtrlOvlArmed()) {
        // bounce right after opening: put the switch back to the current PID state, ignore the command
        if (g_uiSnap.pidOn) lv_obj_add_state(sw, LV_STATE_CHECKED);
        else                lv_obj_remove_state(sw, LV_STATE_CHECKED);
        return;
    }
    uiCtrlTouchPopupGuard();
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    g_uiSnap.pidOn = on;
    if (on) {
        if (g_uiSnap.pidSv <= 0.0f) g_uiSnap.pidSv = 180.0f;
        if (g_uiSnap.batchLive && g_uiSnap.airPct < 10) g_uiSnap.airPct = 10;
    }
    if (g_uiOps.setPidOn) g_uiOps.setPidOn(on);
    uiCtrlRefreshOvlVal();
}

static inline void uiCtrlPrepPopupBtn(lv_obj_t *btn) {
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLL_CHAIN);
    // no translate_y, so the touch area stays fixed while pressing repeatedly on the CST816S
    lv_obj_set_style_translate_y(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
}

static inline void uiCtrlOpenOverlay(UiCtrlOverlay mode) {
    uiCtrlCloseOverlay();
    g_uiCtrl.ovlMode = mode;
    uiCtrlTouchPopupGuard();
    g_uiCtrl.ovlArmAt = millis() + UI_OVL_ARM_MS;
    bool isHtr = (mode == UI_OVL_HEATER);
    UiTone tone = isHtr ? UI_TONE_ORANGE : UI_TONE_CYAN;
    uint32_t col = isHtr ? UI_HEX_ORANGE : UI_HEX_CYAN;

    lv_obj_t *bg = lv_obj_create(g_uiCtrl.shell.root);
    lv_obj_remove_style_all(bg);
    lv_obj_set_size(bg, UI_SCR_W, UI_SCR_H);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bg, LV_OPA_50, LV_PART_MAIN);
    lv_obj_add_flag(bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(bg, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(bg, uiOvlBgClickCb, LV_EVENT_CLICKED, nullptr);
    g_uiCtrl.ovlBackdrop = bg;

    // Bottom-half popup frame: 230 px wide, 136 px high
    const int32_t panelH = 136;
    lv_obj_t *p = uiCreateCard(bg, UI_CONTENT_W, panelH);
    lv_obj_set_pos(p, UI_MARGIN, UI_SCR_H - UI_MARGIN - panelH);
    lv_obj_set_style_radius(p, UI_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_border_width(p, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(p, uiToneColor(tone), LV_PART_MAIN);
    lv_obj_set_style_pad_all(p, UI_PAD_IN, LV_PART_MAIN);
    lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    g_uiCtrl.ovlPanel = p;

    // ---- ROW 1: free area above the 3 buttons = 136 - 4(border) - 10(pad) - 64(button) = 58 px.
    // The group (icon + value) is spaced evenly from the top, bottom and left edge of this area; the PID group evenly from top, bottom and right. ----
    const int32_t rowH = 136 - 4 - 2 * UI_PAD_IN - 64;          // 58
    const int32_t m1   = (rowH - UI_CTRL_ICON_PX) / 2;            // 10 px (top = bottom = left = right)
    const int32_t mx = m1 - 2;   // icon 2 px left, PID switch 2 px right (same as the Heater/Air buttons on the page)
    if (isHtr) {
        g_uiCtrl.ovlIco = uiIconV3Create(p, UIICON_FLAME);
        lv_obj_align(g_uiCtrl.ovlIco, LV_ALIGN_TOP_LEFT, mx, m1);
        g_uiCtrl.ovlPidTag = uiCtrlPidTagCreate(p, g_uiCtrl.ovlIco);
    } else {
        lv_obj_t *ico = uiIconV3Create(p, UIICON_FAN);
        lv_obj_align(ico, LV_ALIGN_TOP_LEFT, mx, m1);
    }

    g_uiCtrl.ovlVal = lv_label_create(p);
    lv_obj_set_style_text_font(g_uiCtrl.ovlVal, UI_FONT_C_HV, LV_PART_MAIN);   // same value font as the Heater/Air buttons
    lv_obj_align(g_uiCtrl.ovlVal, LV_ALIGN_TOP_LEFT, mx + UI_CTRL_ICON_PX + 8, m1 + (UI_CTRL_ICON_PX - lv_font_get_line_height(UI_FONT_C_HV)) / 2);

    g_uiCtrl.ovlValUnit = lv_label_create(p);
    lv_obj_set_style_text_font(g_uiCtrl.ovlValUnit, UI_FONT_T5, LV_PART_MAIN);
    uiCtrlRefreshOvlVal();

    if (isHtr) {
        g_uiCtrl.ovlSwPid = uiCreateIosSwitch(p, UI_TONE_GREEN);
        lv_obj_set_size(g_uiCtrl.ovlSwPid, 56, 30);
        lv_obj_align(g_uiCtrl.ovlSwPid, LV_ALIGN_TOP_RIGHT, -mx, m1 + (UI_CTRL_ICON_PX - 30) / 2);
        lv_obj_remove_flag(g_uiCtrl.ovlSwPid, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_set_ext_click_area(g_uiCtrl.ovlSwPid, 12);
        if (g_uiSnap.pidOn) lv_obj_add_state(g_uiCtrl.ovlSwPid, LV_STATE_CHECKED);
        lv_obj_add_event_cb(g_uiCtrl.ovlSwPid, uiOvlPidCb, LV_EVENT_VALUE_CHANGED, nullptr);
    }

    // ---- ROW 2: 3 large separated buttons (64 px high, 6 px gap): OFF | - | + ----
    // Width inside the panel: 230 - 4(border) - 10(pad) = 216 px -> 66 + 6 + 72 + 6 + 66 = 216 px
    const int32_t bw  = 66;
    const int32_t bwm = 72;
    const int32_t bh  = 64;

    UiToneBtn bOff = uiCreateToneBtn(p, bw, bh, UI_TONE_RED);
    lv_obj_align(bOff.btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    uiCtrlPrepPopupBtn(bOff.btn);
    uiCtrlEndStyle(bOff.btn);   // END button style: solid bright orange-red fill, black text
    lv_obj_add_event_cb(bOff.btn, uiOvlOffCb, LV_EVENT_CLICKED, nullptr);   // OFF: needs touch + release to turn off (avoids accidental off)
    for (int k = 0; k < 2; k++) {   // OFF text 1 px bolder: two text layers, the second 1 px to the right
        lv_obj_t *lo = lv_label_create(bOff.btn);
        lv_label_set_text(lo, uiTr(STR_OFF));
        lv_obj_set_style_text_font(lo, UI_FONT_T3, LV_PART_MAIN);
        lv_obj_set_style_text_color(lo, lv_color_hex(UI_HEX_BG), LV_PART_MAIN);
        lv_obj_align(lo, LV_ALIGN_CENTER, k, 0);
    }

    UiToneBtn bMinus = uiCreateToneBtn(p, bwm, bh, tone);
    lv_obj_align(bMinus.btn, LV_ALIGN_BOTTOM_MID, 0, 0);
    uiCtrlPrepPopupBtn(bMinus.btn);
    uiCtrlSolidBtn(bMinus.btn, col);   // END button style: solid tone color (Heater orange / Air blue), black text
    lv_obj_add_event_cb(bMinus.btn, uiOvlMinusCb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(bMinus.btn, uiOvlMinusCb, LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
    lv_obj_t *lm = lv_label_create(bMinus.btn);
    lv_label_set_text(lm, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(lm, UI_FONT_T2, LV_PART_MAIN);
    lv_obj_set_style_text_color(lm, lv_color_hex(UI_HEX_BG), LV_PART_MAIN);
    lv_obj_center(lm);

    UiToneBtn bPlus = uiCreateToneBtn(p, bw, bh, tone);
    lv_obj_align(bPlus.btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    uiCtrlPrepPopupBtn(bPlus.btn);
    uiCtrlSolidBtn(bPlus.btn, col);
    lv_obj_add_event_cb(bPlus.btn, uiOvlPlusCb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(bPlus.btn, uiOvlPlusCb, LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
    lv_obj_t *lp = lv_label_create(bPlus.btn);
    lv_label_set_text(lp, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(lp, UI_FONT_T2, LV_PART_MAIN);
    lv_obj_set_style_text_color(lp, lv_color_hex(UI_HEX_BG), LV_PART_MAIN);
    lv_obj_center(lp);
}

// ---------------- CONTROL CALLBACKS ----------------
static void uiCtrlHeaterCb(lv_event_t *e) { (void)e; uiCtrlOpenOverlay(UI_OVL_HEATER); }
static void uiCtrlAirCb(lv_event_t *e)    { (void)e; uiCtrlOpenOverlay(UI_OVL_AIR); }
static void uiCtrlDrumCb(lv_event_t *e)   { (void)e; if (g_uiOps.toggleDrum) g_uiOps.toggleDrum(); }
static void uiCtrlCoolCb(lv_event_t *e)   { (void)e; if (g_uiOps.toggleCool) g_uiOps.toggleCool(); }

static void uiCtrlEndConfirmedCb() {
    if (g_uiOps.endBatch) g_uiOps.endBatch();
    uiNavGo(UI_PAGE_HOME);
}

static void uiCtrlEndCb(lv_event_t *e) {
    (void)e;
    if (!g_uiSnap.batchLive) {
        uiNavGo(UI_PAGE_HOME);
        return;
    }
    uiConfirmOpen(g_uiCtrl.confirmEnd, g_uiCtrl.shell.root, uiTr(STR_CONFIRM_END), uiCtrlEndConfirmedCb);
}

static inline void uiBuildControl(lv_obj_t *scr) {
    g_uiCtrl = UiControlState{};
    const char *modeLbl = !g_uiSnap.batchLive ? uiTr(STR_T_IDLE)
                        : uiTr(STR_T_MANUAL);
    g_uiCtrl.shell = uiCreateShell(scr, modeLbl);
    if (g_uiCtrl.shell.lblMid) {
        lv_obj_set_style_text_color(g_uiCtrl.shell.lblMid, lv_color_hex(UI_HEX_TEXT), LV_PART_MAIN);
        lv_label_set_text(g_uiCtrl.shell.lblMid, "");
    }

    // Row 1 - plain telemetry (72 px high): orange SIM/LIVE cell top-left + lowered BT group below | RoR right
    lv_obj_t *tele = lv_obj_create(g_uiCtrl.shell.body);
    lv_obj_remove_style_all(tele);
    lv_obj_set_size(tele, UI_CONTENT_W, 72);
    lv_obj_set_pos(tele, 0, 0);
    lv_obj_set_style_bg_opa(tele, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_side(tele, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_width(tele, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(tele, lv_color_hex(UI_HEX_DIVIDER), LV_PART_MAIN);
    lv_obj_remove_flag(tele, LV_OBJ_FLAG_SCROLLABLE);

    // Orange rectangle with SIM or LIVE text: same as the EVENT page (right edge fixed at 152 px, aligned with the CH / COOL right edge)
    g_uiCtrl.badgeSimLive = lv_obj_create(tele);
    lv_obj_remove_style_all(g_uiCtrl.badgeSimLive);
    lv_obj_set_size(g_uiCtrl.badgeSimLive, LV_SIZE_CONTENT, 20);   // 2 px lower than before
    lv_obj_set_style_bg_color(g_uiCtrl.badgeSimLive, lv_color_hex(UI_HEX_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_uiCtrl.badgeSimLive, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_uiCtrl.badgeSimLive, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(g_uiCtrl.badgeSimLive, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(g_uiCtrl.badgeSimLive, 0, LV_PART_MAIN);
    lv_obj_align(g_uiCtrl.badgeSimLive, LV_ALIGN_TOP_RIGHT, -(UI_COL3_W + UI_GAP), 1);   // same as EVENT

    g_uiCtrl.lblSimLive = lv_label_create(g_uiCtrl.badgeSimLive);
    lv_label_set_text(g_uiCtrl.lblSimLive, g_uiSnap.simMode ? "SIM" : "LIVE");
    lv_obj_set_style_text_font(g_uiCtrl.lblSimLive, UI_FONT_C_UNIT, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiCtrl.lblSimLive, lv_color_hex(UI_HEX_BG), LV_PART_MAIN);
    lv_obj_center(g_uiCtrl.lblSimLive);

    // BT number: large integer + ~7/10 decimal + unit right after; all parts share one text baseline with the RoR number
    g_uiCtrl.lblBt = lv_label_create(tele);
    lv_label_set_text(g_uiCtrl.lblBt, "--");
    lv_obj_set_style_text_font(g_uiCtrl.lblBt, UI_FONT_C_BT, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiCtrl.lblBt, lv_color_hex(UI_HEX_ORANGE), LV_PART_MAIN);

    g_uiCtrl.lblBtDec = lv_label_create(tele);
    lv_label_set_text(g_uiCtrl.lblBtDec, ".-");
    lv_obj_set_style_text_font(g_uiCtrl.lblBtDec, UI_FONT_C_BTD, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiCtrl.lblBtDec, lv_color_hex(UI_HEX_ORANGE), LV_PART_MAIN);

    g_uiCtrl.lblBtUnit = lv_label_create(tele);
    lv_label_set_text(g_uiCtrl.lblBtUnit, uiTempUnit());
    lv_obj_set_style_text_font(g_uiCtrl.lblBtUnit, UI_FONT_C_UNIT, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiCtrl.lblBtUnit, lv_color_hex(UI_HEX_TEXT_SUB), LV_PART_MAIN);
    uiCtrlBtLayout();

    lv_obj_t *lrorTag = lv_label_create(tele);
    lv_label_set_text(lrorTag, "RoR");
    lv_obj_set_style_text_font(lrorTag, UI_FONT_C_TAG, LV_PART_MAIN);
    lv_obj_set_style_text_color(lrorTag, lv_color_hex(UI_HEX_TEXT_SUB), LV_PART_MAIN);
    lv_obj_align(lrorTag, LV_ALIGN_TOP_RIGHT, -2, 4);

    g_uiCtrl.lblRor = lv_label_create(tele);
    lv_label_set_text(g_uiCtrl.lblRor, "_");
    lv_obj_set_style_text_font(g_uiCtrl.lblRor, UI_FONT_C_VAL, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiCtrl.lblRor, lv_color_hex(UI_HEX_GREEN), LV_PART_MAIN);

    g_uiCtrl.lblRorDec = lv_label_create(tele);
    lv_label_set_text(g_uiCtrl.lblRorDec, "._");
    lv_obj_set_style_text_font(g_uiCtrl.lblRorDec, UI_FONT_C_VALD, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiCtrl.lblRorDec, lv_color_hex(UI_HEX_GREEN), LV_PART_MAIN);
    uiCtrlRorLayout();

    // Row 2 - HEATER (112 px) | 5 px | AIR (113 px), 62 px high
    const int32_t y2 = 72 + UI_GAP;
    g_uiCtrl.btnHeater = uiCreateToneBtn(g_uiCtrl.shell.body, UI_COL2_W, 62, UI_TONE_ORANGE);
    lv_obj_set_pos(g_uiCtrl.btnHeater.btn, 0, y2);
    lv_obj_add_event_cb(g_uiCtrl.btnHeater.btn, uiCtrlHeaterCb, LV_EVENT_CLICKED, nullptr);

    g_uiCtrl.imgHeater = uiIconV3Create(g_uiCtrl.btnHeater.btn, UIICON_FLAME);
    lv_obj_align(g_uiCtrl.imgHeater, LV_ALIGN_LEFT_MID, UI_CTRL_BTN_PADX, 0);
    uiIconV3SetOn(g_uiCtrl.imgHeater, true, UI_HEX_RED_OFF);      // icon always lit (recolored to the END orange-red)
    g_uiCtrl.tagHeater = uiCtrlPidTagCreate(g_uiCtrl.btnHeater.btn, g_uiCtrl.imgHeater);

    g_uiCtrl.lblHeater = lv_label_create(g_uiCtrl.btnHeater.btn);
    lv_label_set_text(g_uiCtrl.lblHeater, "0");
    lv_obj_set_style_text_font(g_uiCtrl.lblHeater, UI_FONT_C_HV, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiCtrl.lblHeater, lv_color_hex(UI_HEX_TEXT_SUB), LV_PART_MAIN);
    lv_obj_align(g_uiCtrl.lblHeater, LV_ALIGN_RIGHT_MID, -UI_CTRL_BTN_PADX, 0);

    g_uiCtrl.lblHeaterUnit = lv_label_create(g_uiCtrl.btnHeater.btn);
    lv_label_set_text(g_uiCtrl.lblHeaterUnit, "%");
    lv_obj_set_style_text_font(g_uiCtrl.lblHeaterUnit, UI_FONT_C_UNIT, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiCtrl.lblHeaterUnit, lv_color_hex(UI_HEX_TEXT_SUB), LV_PART_MAIN);
    lv_obj_add_flag(g_uiCtrl.lblHeaterUnit, LV_OBJ_FLAG_HIDDEN);   // no unit in the button (no "%"; a 3-digit SV needs the room)

    g_uiCtrl.btnAir = uiCreateToneBtn(g_uiCtrl.shell.body, UI_COL2_W_R, 62, UI_TONE_CYAN);
    lv_obj_set_pos(g_uiCtrl.btnAir.btn, UI_COL2_W + UI_GAP, y2);
    lv_obj_add_event_cb(g_uiCtrl.btnAir.btn, uiCtrlAirCb, LV_EVENT_CLICKED, nullptr);

    g_uiCtrl.imgAir = uiIconV3Create(g_uiCtrl.btnAir.btn, UIICON_FAN);
    lv_obj_align(g_uiCtrl.imgAir, LV_ALIGN_LEFT_MID, UI_CTRL_BTN_PADX, 0);
    uiIconV3SetOn(g_uiCtrl.imgAir, true, UI_HEX_CYAN_OFF);

    g_uiCtrl.lblAir = lv_label_create(g_uiCtrl.btnAir.btn);
    lv_label_set_text(g_uiCtrl.lblAir, "0");
    lv_obj_set_style_text_font(g_uiCtrl.lblAir, UI_FONT_C_HV, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiCtrl.lblAir, lv_color_hex(UI_HEX_TEXT_SUB), LV_PART_MAIN);
    lv_obj_align(g_uiCtrl.lblAir, LV_ALIGN_RIGHT_MID, -UI_CTRL_BTN_PADX, 0);

    // Row 3 - DRUM (73 px) | COOL (74 px) | END (73 px), 58 px high
    const int32_t y3 = y2 + 62 + UI_GAP;
    const int32_t w1 = UI_COL3_W;
    const int32_t w2 = UI_COL3_W_M;

    g_uiCtrl.btnDrum = uiCreateToneBtn(g_uiCtrl.shell.body, w1, 58, UI_TONE_GREEN);
    lv_obj_set_pos(g_uiCtrl.btnDrum.btn, 0, y3);
    lv_obj_add_event_cb(g_uiCtrl.btnDrum.btn, uiCtrlDrumCb, LV_EVENT_CLICKED, nullptr);
    g_uiCtrl.imgDrum = uiIconV3Create(g_uiCtrl.btnDrum.btn, UIICON_DRUM);
    lv_obj_center(g_uiCtrl.imgDrum);
    uiIconV3SetOn(g_uiCtrl.imgDrum, true, UI_HEX_GREEN_OFF);

    g_uiCtrl.btnCool = uiCreateToneBtn(g_uiCtrl.shell.body, w2, 58, UI_TONE_PERI);
    lv_obj_set_pos(g_uiCtrl.btnCool.btn, w1 + UI_GAP, y3);
    lv_obj_add_event_cb(g_uiCtrl.btnCool.btn, uiCtrlCoolCb, LV_EVENT_CLICKED, nullptr);
    g_uiCtrl.imgCool = uiIconV3Create(g_uiCtrl.btnCool.btn, UIICON_SNOW);
    lv_obj_center(g_uiCtrl.imgCool);
    uiIconV3SetOn(g_uiCtrl.imgCool, true, UI_HEX_PERI_OFF);

    g_uiCtrl.btnEnd = uiCreateToneBtn(g_uiCtrl.shell.body, w1, 58, UI_TONE_RED);
    lv_obj_set_pos(g_uiCtrl.btnEnd.btn, w1 + UI_GAP + w2 + UI_GAP, y3);
    lv_obj_add_event_cb(g_uiCtrl.btnEnd.btn, uiCtrlEndCb, LV_EVENT_CLICKED, nullptr);
    // END button: solid bright red, black "END" text (text as tall as the other buttons' icons)
    uiCtrlEndStyle(g_uiCtrl.btnEnd.btn);
    g_uiCtrl.imgEnd = lv_label_create(g_uiCtrl.btnEnd.btn);
    lv_label_set_text(g_uiCtrl.imgEnd, uiTr(STR_END));
    lv_obj_set_style_text_font(g_uiCtrl.imgEnd, UI_FONT_C_END, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiCtrl.imgEnd, lv_color_hex(UI_HEX_BG), LV_PART_MAIN);
    lv_obj_center(g_uiCtrl.imgEnd);
}

static inline void uiSyncControl() {
    const char *modeLbl = !g_uiSnap.batchLive ? uiTr(STR_T_IDLE)
                        : uiTr(STR_T_MANUAL);
    if (g_uiCtrl.shell.lblLeft) lv_label_set_text(g_uiCtrl.shell.lblLeft, modeLbl);
    uiUpdateTopRight(g_uiCtrl.shell, g_uiSnap.batPct, g_uiSnap.batCharging);

    // TopBar: while roasting show the MM:SS roast timer; otherwise the HH:MM clock if synced
    if (g_uiCtrl.shell.lblMid) {
        if (g_uiSnap.batchLive) {
            if (g_uiSnap.roastSec != g_uiCtrl.lastSec || g_uiCtrl.lastLive != 1) {
                g_uiCtrl.lastSec = g_uiSnap.roastSec;
                g_uiCtrl.lastClock[0] = '\0';
                lv_obj_set_style_text_color(g_uiCtrl.shell.lblMid, lv_color_hex(UI_HEX_TEXT), LV_PART_MAIN);
                lv_label_set_text_fmt(g_uiCtrl.shell.lblMid, "%02lu:%02lu",
                                      static_cast<unsigned long>(g_uiSnap.roastSec / 60),
                                      static_cast<unsigned long>(g_uiSnap.roastSec % 60));
                uiShellCenterMid(g_uiCtrl.shell);
            }
        } else {
            const char *clk = g_uiSnap.clockSynced ? g_uiSnap.clockStr : "";
            if (g_uiCtrl.lastLive != 0 || strcmp(g_uiCtrl.lastClock, clk) != 0) {
                snprintf(g_uiCtrl.lastClock, sizeof(g_uiCtrl.lastClock), "%s", clk);
                g_uiCtrl.lastSec = 0xFFFFFFFF;
                lv_obj_set_style_text_color(g_uiCtrl.shell.lblMid, lv_color_hex(UI_HEX_TEXT), LV_PART_MAIN);
                lv_label_set_text(g_uiCtrl.shell.lblMid, clk);
                if (clk[0]) uiShellCenterMid(g_uiCtrl.shell);
            }
        }
    }
    g_uiCtrl.lastLive = g_uiSnap.batchLive ? 1 : 0;

    int curSim = g_uiSnap.simMode ? 1 : 0;
    if (curSim != g_uiCtrl.lastSim && g_uiCtrl.lblSimLive) {
        g_uiCtrl.lastSim = curSim;
        lv_label_set_text(g_uiCtrl.lblSimLive, g_uiSnap.simMode ? "SIM" : "LIVE");
        if (g_uiCtrl.badgeSimLive) {
            lv_obj_align(g_uiCtrl.badgeSimLive, LV_ALIGN_TOP_RIGHT, -(UI_COL3_W + UI_GAP), 1);
        }
    }

    int bt10 = static_cast<int>(uiTempDisp(g_uiSnap.bt) * 10.0f);
    if (g_uiCtrl.lblBtUnit && g_uiCtrl.lastUnitF != (g_uiSnap.unitF ? 1 : 0)) {
        g_uiCtrl.lastUnitF = g_uiSnap.unitF ? 1 : 0;
        lv_label_set_text(g_uiCtrl.lblBtUnit, uiTempUnit());
        uiCtrlBtLayout();
        g_uiCtrl.lastBt10  = INT32_MIN;   // force a redraw of BT / RoR / SV in the new unit
        g_uiCtrl.lastRor10 = INT32_MIN;
        g_uiCtrl.lastPidSv = INT32_MIN;
    }
    if (bt10 != g_uiCtrl.lastBt10 && g_uiCtrl.lblBt) {
        g_uiCtrl.lastBt10 = bt10;
        lv_label_set_text_fmt(g_uiCtrl.lblBt, "%s%d", bt10 < 0 ? "-" : "", abs(bt10) / 10);
        lv_label_set_text_fmt(g_uiCtrl.lblBtDec, ".%d", abs(bt10) % 10);
        {   // BT number color: cold (blue) -> hot (red) by °C, from UI_TEMP_COLOR_FROM to UI_TEMP_COLOR_TO; red above the overheat level
            const lv_color_t col = uiBtColor(g_uiSnap.bt, (float)g_uiSnap.overheatC);
            lv_obj_set_style_text_color(g_uiCtrl.lblBt,    col, LV_PART_MAIN);
            lv_obj_set_style_text_color(g_uiCtrl.lblBtDec, col, LV_PART_MAIN);
        }
        uiCtrlBtLayout();   // unit right after the number, position by text width
    }

    int ror10 = static_cast<int>(lroundf(uiRorDisp(g_uiSnap.ror) * 10.0f));
    if (ror10 != g_uiCtrl.lastRor10 && g_uiCtrl.lblRor) {
        g_uiCtrl.lastRor10 = ror10;
        if (ror10 >= 5) {   // show RoR only when RoR >= 0.5 (negative or smaller shows _._)
            lv_label_set_text_fmt(g_uiCtrl.lblRor, "+%d", ror10 / 10);
            lv_label_set_text_fmt(g_uiCtrl.lblRorDec, ".%d", ror10 % 10);
        } else {
            lv_label_set_text(g_uiCtrl.lblRor, "_");
            lv_label_set_text(g_uiCtrl.lblRorDec, "._");
        }
        uiCtrlRorLayout();
    }

    const bool holdPopupSync = (g_uiCtrl.ovlMode != UI_OVL_NONE) &&
                               (static_cast<int32_t>(millis() - g_uiCtrl.ovlHoldSyncUntil) < 0);

    int curPidOn = g_uiSnap.pidOn ? 1 : 0;
    int curPidSv = static_cast<int>(lroundf(uiTempDisp(g_uiSnap.pidSv)));
    if ((g_uiSnap.heaterPct != g_uiCtrl.lastHtr || curPidOn != g_uiCtrl.lastPidOn || curPidSv != g_uiCtrl.lastPidSv) && g_uiCtrl.lblHeater) {
        g_uiCtrl.lastHtr   = g_uiSnap.heaterPct;
        g_uiCtrl.lastPidOn = curPidOn;
        g_uiCtrl.lastPidSv = curPidSv;
        bool pidOn = g_uiSnap.pidOn;
        bool on    = pidOn || (g_uiSnap.heaterPct > 0);
        // with PID on, the SV number keeps the Heater orange
        uint32_t valCol = on ? UI_HEX_ORANGE : UI_HEX_TEXT_SUB;

        if (pidOn) {
            lv_label_set_text_fmt(g_uiCtrl.lblHeater, "%d", curPidSv);
        } else {
            lv_label_set_text_fmt(g_uiCtrl.lblHeater, "%d", g_uiSnap.heaterPct);
        }
        lv_obj_align(g_uiCtrl.lblHeater, LV_ALIGN_RIGHT_MID, -UI_CTRL_BTN_PADX, 0);
        lv_obj_set_style_text_color(g_uiCtrl.lblHeater, lv_color_hex(valCol), LV_PART_MAIN);
        if (g_uiCtrl.lblHeaterUnit) {
            lv_obj_set_style_text_color(g_uiCtrl.lblHeaterUnit, lv_color_hex(valCol), LV_PART_MAIN);
            lv_obj_align_to(g_uiCtrl.lblHeaterUnit, g_uiCtrl.lblHeater, LV_ALIGN_OUT_RIGHT_BOTTOM, 1, -4);
        }
        uiIconV3SetOn(g_uiCtrl.imgHeater, true, UI_HEX_RED_OFF);
        uiCtrlPidTagSet(g_uiCtrl.tagHeater, g_uiCtrl.imgHeater, pidOn);
        uiToneBtnSetOn(g_uiCtrl.btnHeater, on);

        if (g_uiCtrl.ovlMode == UI_OVL_HEATER && !holdPopupSync) {
            if (g_uiCtrl.ovlSwPid) {
                if (pidOn) lv_obj_add_state(g_uiCtrl.ovlSwPid, LV_STATE_CHECKED);
                else       lv_obj_remove_state(g_uiCtrl.ovlSwPid, LV_STATE_CHECKED);
            }
            uiCtrlRefreshOvlVal();
        }
    }

    if (g_uiSnap.airPct != g_uiCtrl.lastAir && g_uiCtrl.lblAir) {
        g_uiCtrl.lastAir = g_uiSnap.airPct;
        bool on = (g_uiSnap.airPct > 0);
        lv_label_set_text_fmt(g_uiCtrl.lblAir, "%d", g_uiSnap.airPct);
        lv_obj_set_style_text_color(g_uiCtrl.lblAir,
            lv_color_hex(on ? UI_HEX_CYAN : UI_HEX_TEXT_SUB), LV_PART_MAIN);
        lv_obj_align(g_uiCtrl.lblAir, LV_ALIGN_RIGHT_MID, -UI_CTRL_BTN_PADX, 0);
        uiIconV3SetOn(g_uiCtrl.imgAir, true, UI_HEX_CYAN_OFF);
        uiToneBtnSetOn(g_uiCtrl.btnAir, on);
        if (g_uiCtrl.ovlMode == UI_OVL_AIR && !holdPopupSync) {
            uiCtrlRefreshOvlVal();
        }
    }

    if (static_cast<int>(g_uiSnap.drumOn) != g_uiCtrl.lastDrum) {
        g_uiCtrl.lastDrum = g_uiSnap.drumOn ? 1 : 0;
        uiToneBtnSetOn(g_uiCtrl.btnDrum, g_uiSnap.drumOn);
        uiIconV3SetOn(g_uiCtrl.imgDrum, true, UI_HEX_GREEN_OFF);
    }

    if (static_cast<int>(g_uiSnap.coolOn) != g_uiCtrl.lastCool) {
        g_uiCtrl.lastCool = g_uiSnap.coolOn ? 1 : 0;
        uiToneBtnSetOn(g_uiCtrl.btnCool, g_uiSnap.coolOn);
        uiIconV3SetOn(g_uiCtrl.imgCool, true, UI_HEX_PERI_OFF);
    }

    // END 5 s confirmation popup countdown
    uiConfirmTick(g_uiCtrl.confirmEnd);
}
