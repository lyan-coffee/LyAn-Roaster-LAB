#pragma once
#include <lvgl.h>
#include "UIConfig.h"
#include "UILang.h"
#include "RoasterUIWidgets.h"
#include "RoasterUIBridge.h"   // g_uiSnap: °C/°F unit for displayed temperatures
#include "RoasterAlarmSound.h"   // tier 1: alert speaker (g_alarm_sound)

// ============================================================================
// lib/RoasterUIAlert.h - Full-screen 240x240 alerts: tinted background + 3 px border, 83 px rounded triangle blinking with the !/? mark,
// title, orange temperature value (°C unit as superscript), sub line; the whole group centered, NO button (touch to acknowledge).
// Overheat = red, Signal lost = orange. OTA = black background + orange progress bar (one layout for all 3 states).
// ============================================================================

enum UiAlertMode : uint8_t {
    UI_ALERT_NONE = 0,
    UI_ALERT_OVERHEAT,
    UI_ALERT_SIGNAL_LOST
};

struct UiAlertState {
    UiAlertMode mode       = UI_ALERT_NONE;
    lv_obj_t   *root       = nullptr;
    lv_obj_t   *lblTitle   = nullptr;
    lv_obj_t   *lblTopSub  = nullptr;
    lv_obj_t   *lblBig     = nullptr;
    lv_obj_t   *lblInfo    = nullptr;

    lv_obj_t   *lblUnit    = nullptr;
    int         lastIntVal = -9999;
    void (*onDismissCb)()  = nullptr;
};

static UiAlertState g_uiAlert;

static inline bool uiAlertIsActive()    { return g_uiAlert.mode != UI_ALERT_NONE; }
static inline bool uiAlertBlocksInput() { return false; }

static inline void uiAlertHide() {
    if (g_uiAlert.root) lv_obj_delete(g_uiAlert.root);
    g_uiAlert = UiAlertState{};
}

// Overheat / Signal lost: NO button, a touch anywhere acknowledges. Only calls the command / sets the flag;
// the alert screen is removed on the 100 ms tick (uiSyncTick), never by deleting an object inside its own event.
static void uiAlertTapCb(lv_event_t *e) {
    (void)e;
    if (g_uiAlert.onDismissCb) g_uiAlert.onDismissCb();
}

// ---- Alert sound (the screen stays but the speaker is silent after Heater Off / Dismiss / OK) ----
static uint8_t g_uiAlertSndKind  = ALARM_SND_OFF;
static bool    g_uiAlertSndMuted = false;
static inline void uiAlertSoundUpdate(uint8_t want) {
    if (want != g_uiAlertSndKind) { g_uiAlertSndKind = want; g_uiAlertSndMuted = false; }   // new alert type: the speaker sounds again
    const uint8_t snd = (want != ALARM_SND_OFF && !g_uiAlertSndMuted) ? want : (uint8_t)ALARM_SND_OFF;
    if (snd == ALARM_SND_OFF && alarmIsOneShot(g_alarm_sound)) return;   // a sleep / wake chirp is playing: do not cut it off (a real alert still takes priority)
    g_alarm_sound = snd;
}
static inline void uiAlertSoundMute() {
    g_uiAlertSndMuted = true;
    g_alarm_sound = ALARM_SND_OFF;
}

// Full-screen background (blocks touches to the page below). bgHex = tinted color. tapCb != null: a touch anywhere acknowledges.
static inline void uiAlertCreateBase(UiAlertMode mode, uint32_t bgHex, lv_event_cb_t tapCb = nullptr) {
    uiAlertHide();
    g_uiAlert.mode = mode;

    lv_obj_t *root = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(root);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_size(root, UI_SCR_W, UI_SCR_H);
    lv_obj_set_style_bg_color(root, lv_color_hex(bgHex), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    if (tapCb) lv_obj_add_event_cb(root, tapCb, LV_EVENT_CLICKED, nullptr);
    g_uiAlert.root = root;
}

// 3 px border around the screen: its own object, not touchable, created LAST so it is on top
static inline void uiAlertAddFrame(uint32_t col) {
    lv_obj_t *fr = lv_obj_create(g_uiAlert.root);
    lv_obj_remove_style_all(fr);
    lv_obj_set_pos(fr, 0, 0);
    lv_obj_set_size(fr, UI_SCR_W, UI_SCR_H);
    lv_obj_set_style_border_width(fr, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(fr, lv_color_hex(col), LV_PART_MAIN);
    lv_obj_set_style_border_opa(fr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(fr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(fr, LV_OBJ_FLAG_SCROLLABLE);
}

// ---- 83 px rounded warning triangle ----
// A8 image (alpha only) drawn once into RAM (~6.9 KB): rounded corners, anti-aliased; colored with image_recolor.
#define UI_ALERT_TRI_D  83
static uint8_t          g_uiAlertTriBuf[UI_ALERT_TRI_D * UI_ALERT_TRI_D];
static lv_image_dsc_t   g_uiAlertTriDsc;
static bool             g_uiAlertTriReady = false;

static inline float uiAlertSegDist(float px, float py, float ax, float ay, float bx, float by) {
    const float vx = bx - ax, vy = by - ay;
    float t = ((px - ax) * vx + (py - ay) * vy) / (vx * vx + vy * vy);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    const float dx = px - (ax + t * vx), dy = py - (ay + t * vy);
    return sqrtf(dx * dx + dy * dy);
}

static inline const lv_image_dsc_t* uiAlertTriangleImg() {
    if (!g_uiAlertTriReady) {
        // Inner (shrunk) triangle + corner radius R: final shape = every point within R of the inner triangle
        const float ax = 41.5f, ay = 12.5f, bx = 72.6f, by = 66.4f, cx = 10.4f, cy = 66.4f, R = 6.2f;
        const float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);   // signed area (tells which side is inside)
        for (int y = 0; y < UI_ALERT_TRI_D; y++) {
            for (int x = 0; x < UI_ALERT_TRI_D; x++) {
                const float px = x + 0.5f, py = y + 0.5f;
                float d = uiAlertSegDist(px, py, ax, ay, bx, by);
                float d2 = uiAlertSegDist(px, py, bx, by, cx, cy); if (d2 < d) d = d2;
                d2 = uiAlertSegDist(px, py, cx, cy, ax, ay);       if (d2 < d) d = d2;
                const float s1 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
                const float s2 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
                const float s3 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
                const bool inside = (area > 0) ? (s1 >= 0 && s2 >= 0 && s3 >= 0) : (s1 <= 0 && s2 <= 0 && s3 <= 0);
                const float sd = (inside ? -d : d) - R;                 // < 0: inside the shape
                float cov = 0.5f - sd;
                cov = cov < 0 ? 0 : (cov > 1 ? 1 : cov);
                g_uiAlertTriBuf[y * UI_ALERT_TRI_D + x] = static_cast<uint8_t>(cov * 255.0f + 0.5f);
            }
        }
        memset(&g_uiAlertTriDsc, 0, sizeof(g_uiAlertTriDsc));
        g_uiAlertTriDsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
        g_uiAlertTriDsc.header.cf     = LV_COLOR_FORMAT_A8;
        g_uiAlertTriDsc.header.w      = UI_ALERT_TRI_D;
        g_uiAlertTriDsc.header.h      = UI_ALERT_TRI_D;
        g_uiAlertTriDsc.header.stride = UI_ALERT_TRI_D;
        g_uiAlertTriDsc.data_size     = sizeof(g_uiAlertTriBuf);
        g_uiAlertTriDsc.data          = g_uiAlertTriBuf;
        g_uiAlertTriReady = true;
    }
    return &g_uiAlertTriDsc;
}

// v: 0..1000 (loop). Whole icon opacity oscillates as a triangle wave 40..255, period 1200 ms
static void uiAlertBlinkCb(void *obj, int32_t v) {
    const int32_t tri = (v < 500) ? v : (1000 - v);
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(40 + tri * 215 / 500), LV_PART_MAIN);
}

// Icon: rounded triangle in color `col` + cream !/? mark inside; BOTH blink together (title stays still)
static inline lv_obj_t* uiAlertIcon(lv_obj_t *parent, uint32_t col, const char *glyph) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, UI_ALERT_TRI_D, UI_ALERT_TRI_D);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *img = lv_image_create(box);
    lv_image_set_src(img, uiAlertTriangleImg());
    lv_obj_set_style_image_recolor(img, lv_color_hex(col), LV_PART_MAIN);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(img, 0, 0);

    lv_obj_t *g = lv_label_create(box);
    lv_label_set_text(g, glyph);
    lv_obj_set_style_text_font(g, UI_FONT_T2_LG, LV_PART_MAIN);
    lv_obj_set_style_text_color(g, lv_color_hex(UI_HEX_TEXT), LV_PART_MAIN);
    lv_obj_align(g, LV_ALIGN_CENTER, 0, 9);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, box);
    lv_anim_set_exec_cb(&a, uiAlertBlinkCb);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_duration(&a, 1200);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
    return box;
}

// Content column centered (horizontal + vertical) on the WHOLE screen; lines evenly spaced 5 px
static inline lv_obj_t* uiAlertColumn() {
    lv_obj_t *col = lv_obj_create(g_uiAlert.root);
    lv_obj_remove_style_all(col);
    lv_obj_set_pos(col, UI_MARGIN, 0);
    lv_obj_set_size(col, UI_CONTENT_W, UI_SCR_H);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 5, LV_PART_MAIN);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    return col;
}

static inline lv_obj_t* uiAlertLabel(lv_obj_t *parent, const char *txt, const lv_font_t *font, uint32_t col) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(col), LV_PART_MAIN);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    return l;
}

// Row "big number + superscript unit": number in T1 orange, unit (°C/°F) small, attached to the top of the number
static inline void uiAlertValueRow(lv_obj_t *parent, const char *num) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row, 2, LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    g_uiAlert.lblBig  = uiAlertLabel(row, num, UI_FONT_T1, UI_HEX_ORANGE);
    g_uiAlert.lblUnit = uiAlertLabel(row, uiTempUnit(), UI_FONT_T4, UI_HEX_ORANGE);
    lv_obj_set_style_pad_top(g_uiAlert.lblUnit, 10, LV_PART_MAIN);
}

// 1. OVERHEAT ALERT (red tinted background + red border; no button, touch to acknowledge).
//    Stays until the user touches; if still above the threshold a touch only mutes the speaker (see uiAlertDismissHeatCb).
static inline void uiAlertShowOverheat(float btC, void (*onDismiss)() = nullptr, uint16_t limitC = 0) {
    if (g_uiAlert.mode != UI_ALERT_OVERHEAT) {
        uiAlertCreateBase(UI_ALERT_OVERHEAT, UI_HEX_ALARM_BG, uiAlertTapCb);
        g_uiAlert.onDismissCb = onDismiss;

        lv_obj_t *col = uiAlertColumn();
        uiAlertIcon(col, UI_HEX_RED, "!");
        g_uiAlert.lblTitle = uiAlertLabel(col, uiTr(STR_OVERHEAT), UI_FONT_T4, UI_HEX_RED);
        uiAlertValueRow(col, "");
        g_uiAlert.lblTopSub = uiAlertLabel(col, "", UI_FONT_T5, UI_HEX_TEXT_SUB);
        if (limitC > 0) lv_label_set_text_fmt(g_uiAlert.lblTopSub, "Max %d°",
                                              static_cast<int>(lroundf(uiTempDisp(static_cast<float>(limitC)))));
        uiAlertAddFrame(UI_HEX_RED);
    }

    int bt10 = static_cast<int>(uiTempDisp(btC) * 10.0f);
    if (bt10 != g_uiAlert.lastIntVal && g_uiAlert.lblBig) {
        g_uiAlert.lastIntVal = bt10;
        lv_label_set_text_fmt(g_uiAlert.lblBig, "%d.%d", bt10 / 10, abs(bt10 % 10));
    }
}

// 2. SIGNAL LOST ALERT (same layout as Overheat, orange; touch anywhere to acknowledge; temperature "--.-", Heater already cut)
static inline void uiAlertShowSignalLost(uint32_t lostSec, float lastBtC, void (*onDismiss)() = nullptr) {
    (void)lostSec; (void)lastBtC;
    if (g_uiAlert.mode != UI_ALERT_SIGNAL_LOST) {
        uiAlertCreateBase(UI_ALERT_SIGNAL_LOST, UI_HEX_LOST_BG, uiAlertTapCb);
        g_uiAlert.onDismissCb = onDismiss;

        lv_obj_t *col = uiAlertColumn();
        uiAlertIcon(col, UI_HEX_ORANGE, "?");
        g_uiAlert.lblTitle = uiAlertLabel(col, uiTr(STR_SIGNAL_LOST), UI_FONT_T4, UI_HEX_ORANGE);
        uiAlertValueRow(col, "--.-");
        g_uiAlert.lblInfo = uiAlertLabel(col, uiTr(STR_HEATER_OFF), UI_FONT_T5, UI_HEX_TEXT_SUB);
        uiAlertAddFrame(UI_HEX_ORANGE);
    }
}
