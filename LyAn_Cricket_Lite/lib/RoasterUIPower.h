#pragma once
#include "RoasterUIWidgets.h"
#include "RoasterUIBridge.h"

static lv_obj_t *g_uiPowerOverlay = nullptr;

static inline bool uiPowerIsOpen() {
    return g_uiPowerOverlay != nullptr;
}

static inline void uiPowerClose() {
    if (g_uiPowerOverlay) {
        lv_obj_delete(g_uiPowerOverlay);
        g_uiPowerOverlay = nullptr;
    }
}

static void uiPowerBgClickCb(lv_event_t *e) {
    if (lv_event_get_target(e) == g_uiPowerOverlay) uiPowerClose();
}

static void uiPowerCancelCb(lv_event_t *e) {
    (void)e;
    uiPowerClose();
}

static void uiPowerResetCb(lv_event_t *e) {
    (void)e;
    uiPowerClose();
    if (g_uiOps.systemReset) g_uiOps.systemReset();
}

static void uiPowerOffCb(lv_event_t *e) {
    (void)e;
    uiPowerClose();
    if (g_uiOps.systemPowerOff) g_uiOps.systemPowerOff();
}

// ROAST-page style button: SILVER tone, orange border when pressed, T3 cream text, orange ">"
static inline UiToneBtn uiPowerRowBtn(lv_obj_t *parent, int32_t w, int32_t h, const char *txt, lv_event_cb_t cb) {
    UiToneBtn b = uiCreateToneBtn(parent, w, h, UI_TONE_SILVER);
    lv_obj_set_style_border_color(b.btn, lv_color_hex(UI_HEX_ORANGE), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(b.btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(b.btn);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, UI_FONT_T3, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(UI_HEX_TEXT), LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_t *arr = lv_label_create(b.btn);
    lv_label_set_text(arr, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arr, UI_FONT_T4, LV_PART_MAIN);
    lv_obj_set_style_text_color(arr, lv_color_hex(UI_HEX_ORANGE), LV_PART_MAIN);
    lv_obj_align(arr, LV_ALIGN_RIGHT_MID, -14, 0);
    return b;
}

static inline void uiPowerOpen() {
    if (g_uiPowerOverlay) return;

    g_uiPowerOverlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_uiPowerOverlay);
    lv_obj_set_size(g_uiPowerOverlay, UI_SCR_W, UI_SCR_H);
    lv_obj_set_style_bg_color(g_uiPowerOverlay, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_uiPowerOverlay, LV_OPA_70, LV_PART_MAIN);
    lv_obj_add_flag(g_uiPowerOverlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_uiPowerOverlay, uiPowerBgClickCb, LV_EVENT_CLICKED, nullptr);

    // Modal: 214 x 180 px, pad 5, border 2, radius 7
    // Inner area 200 x 166 px = Reset(54) + gap(5) + PowerOff(54) + gap(5) + Cancel(48)
    lv_obj_t *modal = uiCreateCard(g_uiPowerOverlay, 214, 180);
    lv_obj_align(modal, LV_ALIGN_TOP_MID, 0, UI_TOPBAR_H + (UI_SCR_H - UI_TOPBAR_H - 180) / 2);   // centered below the top bar
    lv_obj_set_style_radius(modal, UI_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(modal, lv_color_hex(UI_HEX_BG), LV_PART_MAIN);   // black background so card-colored buttons stand out
    lv_obj_set_style_border_width(modal, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(modal, lv_color_hex(UI_HEX_RED), LV_PART_MAIN);
    lv_obj_set_style_pad_all(modal, UI_PAD_IN, LV_PART_MAIN);
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);

    const int32_t btnW = 200;

    UiToneBtn btnReset = uiPowerRowBtn(modal, btnW, 54, uiTr(STR_RESET), uiPowerResetCb);
    lv_obj_set_pos(btnReset.btn, 0, 0);

    UiToneBtn btnOff = uiPowerRowBtn(modal, btnW, 54, uiTr(STR_POWER_OFF), uiPowerOffCb);
    lv_obj_set_pos(btnOff.btn, 0, 54 + UI_GAP);

    // Cancel: like BACK (SILVER tone, height 48, centered sub-text) without the "<" glyph
    UiToneBtn btnCancel = uiCreateToneBtn(modal, btnW, 48, UI_TONE_SILVER);
    lv_obj_align(btnCancel.btn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_border_color(btnCancel.btn, lv_color_hex(UI_HEX_ORANGE), LV_PART_MAIN | LV_STATE_PRESSED);   // pressed: orange border, as BACK
    lv_obj_add_event_cb(btnCancel.btn, uiPowerCancelCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l3 = lv_label_create(btnCancel.btn);
    lv_label_set_text(l3, uiTr(STR_CANCEL));
    lv_obj_set_style_text_font(l3, UI_FONT_T3, LV_PART_MAIN);
    lv_obj_set_style_text_color(l3, lv_color_hex(UI_HEX_TEXT_SUB), LV_PART_MAIN);
    lv_obj_center(l3);
}
