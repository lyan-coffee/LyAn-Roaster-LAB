#pragma once
#include "RoasterUIWidgets.h"
#include "RoasterUINav.h"
#include "RoasterUIBridge.h"

struct UiEventState {
    UiShell        shell        = {};
    lv_obj_t      *badgeSimLive = nullptr;
    lv_obj_t      *lblSimLive   = nullptr;
    lv_obj_t      *lblBt        = nullptr;   // BT integer part
    lv_obj_t      *lblBtDec     = nullptr;   // BT decimal part (~7/10)
    lv_obj_t      *lblBtUnit    = nullptr;
    lv_obj_t      *lblDevTime   = nullptr;   // DEV time (MM:SS, orange) in place of the "DEV" label
    lv_obj_t      *lblDevC      = nullptr;
    lv_obj_t      *lblDtr       = nullptr;
    UiToneBtn      btnEv[6]     = {};
    lv_obj_t      *lblEvTime[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    UiConfirmPopup confirmDr    = {};
    int            lastSim      = -1;
    int            lastUnitF    = -1;
    int            lastBt10     = -9999;
    char           lastClock[8] = "";
    uint32_t       lastSec      = 0xFFFFFFFF;
    uint32_t       lastDevSec   = 0xFFFFFFFF;
    int            lastDevC10   = -9999;
    int            lastDtr10    = -9999;
    int8_t         lastLive     = -1;
    bool           lastMark[6]  = {false, false, false, false, false, false};
    uint32_t       lastEvSec[6] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
};

static UiEventState g_uiEvent;

static inline bool uiEventConfirmOpen() {
    return uiConfirmIsOpen(g_uiEvent.confirmDr);
}

static inline void uiEventCloseConfirm() {
    uiConfirmClose(g_uiEvent.confirmDr);
}

static void uiEventDrConfirmedCb() {
    if (g_uiOps.markEvent) g_uiOps.markEvent(5);
}

static void uiEventClickCb(lv_event_t *e) {
    uint8_t idx = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    if (idx == 5) { // DR: open the 5 s confirmation popup
        if (g_uiSnap.evMarked[5]) return;
        uiConfirmOpen(g_uiEvent.confirmDr, g_uiEvent.shell.root, uiTr(STR_CONFIRM_DR), uiEventDrConfirmedCb);
        return;
    }
    if (g_uiOps.markEvent) g_uiOps.markEvent(idx);
}

static inline void uiBuildEvent(lv_obj_t *scr) {
    g_uiEvent = UiEventState{};
    g_uiEvent.shell = uiCreateShell(scr, "EVENT");
    if (g_uiEvent.shell.lblMid) {
        lv_obj_set_style_text_color(g_uiEvent.shell.lblMid, lv_color_hex(UI_HEX_TEXT), LV_PART_MAIN);
        lv_label_set_text(g_uiEvent.shell.lblMid, "");
    }

    // Row 1 - plain telemetry, matching the Control page (72 px high)
    lv_obj_t *tele = lv_obj_create(g_uiEvent.shell.body);
    lv_obj_remove_style_all(tele);
    lv_obj_set_size(tele, UI_CONTENT_W, 72);
    lv_obj_set_pos(tele, 0, 0);
    lv_obj_set_style_bg_opa(tele, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_side(tele, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_width(tele, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(tele, lv_color_hex(UI_HEX_DIVIDER), LV_PART_MAIN);
    lv_obj_remove_flag(tele, LV_OBJ_FLAG_SCROLLABLE);

    // SIM/LIVE cell: right edge aligned with the right edge of the CH button (= UI_CONTENT_W - (UI_COL3_W + UI_GAP) = 152 px)
    g_uiEvent.badgeSimLive = lv_obj_create(tele);
    lv_obj_remove_style_all(g_uiEvent.badgeSimLive);
    lv_obj_set_size(g_uiEvent.badgeSimLive, LV_SIZE_CONTENT, 20);
    lv_obj_set_style_bg_color(g_uiEvent.badgeSimLive, lv_color_hex(UI_HEX_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_uiEvent.badgeSimLive, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_uiEvent.badgeSimLive, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(g_uiEvent.badgeSimLive, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(g_uiEvent.badgeSimLive, 0, LV_PART_MAIN);
    lv_obj_align(g_uiEvent.badgeSimLive, LV_ALIGN_TOP_RIGHT, -(UI_COL3_W + UI_GAP), 1);

    g_uiEvent.lblSimLive = lv_label_create(g_uiEvent.badgeSimLive);
    lv_label_set_text(g_uiEvent.lblSimLive, g_uiSnap.simMode ? "SIM" : "LIVE");
    lv_obj_set_style_text_font(g_uiEvent.lblSimLive, UI_FONT_C_UNIT, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiEvent.lblSimLive, lv_color_hex(UI_HEX_BG), LV_PART_MAIN);
    lv_obj_center(g_uiEvent.lblSimLive);

    g_uiEvent.lblBt = lv_label_create(tele);
    lv_label_set_text(g_uiEvent.lblBt, "--");
    lv_obj_set_style_text_font(g_uiEvent.lblBt, UI_FONT_C_BT, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiEvent.lblBt, lv_color_hex(UI_HEX_ORANGE), LV_PART_MAIN);

    g_uiEvent.lblBtDec = lv_label_create(tele);
    lv_label_set_text(g_uiEvent.lblBtDec, ".-");
    lv_obj_set_style_text_font(g_uiEvent.lblBtDec, UI_FONT_C_BTD, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiEvent.lblBtDec, lv_color_hex(UI_HEX_ORANGE), LV_PART_MAIN);

    g_uiEvent.lblBtUnit = lv_label_create(tele);
    lv_label_set_text(g_uiEvent.lblBtUnit, uiTempUnit());
    lv_obj_set_style_text_font(g_uiEvent.lblBtUnit, UI_FONT_C_UNIT, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiEvent.lblBtUnit, lv_color_hex(UI_HEX_TEXT_SUB), LV_PART_MAIN);
    uiBtRowLayout(g_uiEvent.lblBt, g_uiEvent.lblBtDec, g_uiEvent.lblBtUnit, UI_FONT_C_BT, UI_FONT_C_BTD, UI_FONT_C_UNIT, -4);

    // Right DEV group (3 lines of the same size UI_FONT_T3, ~8 px line gap): DEV time (orange) | +ΔT° (cream) | % DTR (green)
    g_uiEvent.lblDevTime = lv_label_create(tele);
    lv_label_set_text(g_uiEvent.lblDevTime, "00:00");
    lv_obj_set_style_text_font(g_uiEvent.lblDevTime, UI_FONT_T3, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiEvent.lblDevTime, lv_color_hex(UI_HEX_ORANGE), LV_PART_MAIN);
    lv_obj_align(g_uiEvent.lblDevTime, LV_ALIGN_TOP_RIGHT, -2, -5);

    g_uiEvent.lblDevC = lv_label_create(tele);
    lv_label_set_text(g_uiEvent.lblDevC, "+0.0°");
    lv_obj_set_style_text_font(g_uiEvent.lblDevC, UI_FONT_T3, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiEvent.lblDevC, lv_color_hex(UI_HEX_TEXT), LV_PART_MAIN);
    lv_obj_align(g_uiEvent.lblDevC, LV_ALIGN_TOP_RIGHT, -2, 17);

    g_uiEvent.lblDtr = lv_label_create(tele);
    lv_label_set_text(g_uiEvent.lblDtr, "0.0%");
    lv_obj_set_style_text_font(g_uiEvent.lblDtr, UI_FONT_T3, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_uiEvent.lblDtr, lv_color_hex(UI_HEX_GREEN), LV_PART_MAIN);
    lv_obj_align(g_uiEvent.lblDtr, LV_ALIGN_BOTTOM_RIGHT, -2, -4);

    // Rows 2 & 3 - 6 events (2x3, 73/74/73 px wide, 60 px high): no logo, name T2 (0, 7), time T5 (0, -6)
    const char *evNames[6] = {"PR", "CH", "DE", "1FC", "2FC", "DR"};
    const UiTone evTones[6] = {
        UI_TONE_ORANGE,
        UI_TONE_ORANGE,
        UI_TONE_ORANGE,
        UI_TONE_ORANGE,
        UI_TONE_ORANGE,
        UI_TONE_RED
    };

    const int32_t w1 = UI_COL3_W;
    const int32_t w2 = UI_COL3_W_M;
    const int32_t h  = 60;

    for (int i = 0; i < 6; ++i) {
        int col = i % 3;
        int row = i / 3;
        int32_t x = (col == 0) ? 0 : (col == 1 ? w1 + UI_GAP : w1 + UI_GAP + w2 + UI_GAP);
        int32_t y = 72 + UI_GAP + row * (h + UI_GAP);
        int32_t w = (col == 1) ? w2 : w1;

        g_uiEvent.btnEv[i] = uiCreateToneBtn(g_uiEvent.shell.body, w, h, evTones[i]);
        lv_obj_set_pos(g_uiEvent.btnEv[i].btn, x, y);
        lv_obj_add_event_cb(g_uiEvent.btnEv[i].btn, uiEventClickCb, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(i)));

        lv_obj_t *lbl = lv_label_create(g_uiEvent.btnEv[i].btn);
        lv_label_set_text(lbl, evNames[i]);
        lv_obj_set_style_text_font(lbl, UI_FONT_T2, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(i == 5 ? UI_HEX_RED : UI_HEX_TEXT), LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 4);   // moved up 3 px

        lv_obj_t *lt = lv_label_create(g_uiEvent.btnEv[i].btn);
        lv_label_set_text(lt, "--:--");
        lv_obj_set_style_text_font(lt, UI_FONT_T5, LV_PART_MAIN);
        lv_obj_set_style_text_color(lt, lv_color_hex(UI_HEX_TEXT_MUTED), LV_PART_MAIN);
        lv_obj_align(lt, LV_ALIGN_BOTTOM_MID, 0, -5);   // timer moved down 1 px
        g_uiEvent.lblEvTime[i] = lt;
    }
}

static inline void uiSyncEvent() {
    uiUpdateTopRight(g_uiEvent.shell, g_uiSnap.batPct, g_uiSnap.batCharging);

    // TopBar: while roasting show the MM:SS roast timer; otherwise the HH:MM clock if synced
    if (g_uiEvent.shell.lblMid) {
        if (g_uiSnap.batchLive) {
            if (g_uiSnap.roastSec != g_uiEvent.lastSec || g_uiEvent.lastLive != 1) {
                g_uiEvent.lastSec = g_uiSnap.roastSec;
                g_uiEvent.lastClock[0] = '\0';
                lv_obj_set_style_text_color(g_uiEvent.shell.lblMid, lv_color_hex(UI_HEX_TEXT), LV_PART_MAIN);
                lv_label_set_text_fmt(g_uiEvent.shell.lblMid, "%02lu:%02lu",
                                      static_cast<unsigned long>(g_uiSnap.roastSec / 60),
                                      static_cast<unsigned long>(g_uiSnap.roastSec % 60));
                uiShellCenterMid(g_uiEvent.shell);
            }
        } else {
            const char *clk = g_uiSnap.clockSynced ? g_uiSnap.clockStr : "";
            if (g_uiEvent.lastLive != 0 || strcmp(g_uiEvent.lastClock, clk) != 0) {
                snprintf(g_uiEvent.lastClock, sizeof(g_uiEvent.lastClock), "%s", clk);
                g_uiEvent.lastSec = 0xFFFFFFFF;
                lv_obj_set_style_text_color(g_uiEvent.shell.lblMid, lv_color_hex(UI_HEX_TEXT), LV_PART_MAIN);
                lv_label_set_text(g_uiEvent.shell.lblMid, clk);
                if (clk[0]) uiShellCenterMid(g_uiEvent.shell);
            }
        }
        g_uiEvent.lastLive = g_uiSnap.batchLive ? 1 : 0;
    }

    int curSim = g_uiSnap.simMode ? 1 : 0;
    if (curSim != g_uiEvent.lastSim && g_uiEvent.lblSimLive) {
        g_uiEvent.lastSim = curSim;
        lv_label_set_text(g_uiEvent.lblSimLive, g_uiSnap.simMode ? "SIM" : "LIVE");
        if (g_uiEvent.badgeSimLive) {
            lv_obj_align(g_uiEvent.badgeSimLive, LV_ALIGN_TOP_RIGHT, -(UI_COL3_W + UI_GAP), 1);
        }
    }

    const int unitF = g_uiSnap.unitF ? 1 : 0;
    int bt10 = static_cast<int>(uiTempDisp(g_uiSnap.bt) * 10.0f);
    if (g_uiEvent.lblBt && (bt10 != g_uiEvent.lastBt10 || unitF != g_uiEvent.lastUnitF)) {
        g_uiEvent.lastBt10  = bt10;
        g_uiEvent.lastUnitF = unitF;
        lv_label_set_text_fmt(g_uiEvent.lblBt, "%s%d", bt10 < 0 ? "-" : "", abs(bt10) / 10);
        lv_label_set_text_fmt(g_uiEvent.lblBtDec, ".%d", abs(bt10) % 10);
        lv_label_set_text(g_uiEvent.lblBtUnit, uiTempUnit());
        const lv_color_t col = uiBtColor(g_uiSnap.bt, (float)g_uiSnap.overheatC);
        lv_obj_set_style_text_color(g_uiEvent.lblBt,    col, LV_PART_MAIN);
        lv_obj_set_style_text_color(g_uiEvent.lblBtDec, col, LV_PART_MAIN);
        uiBtRowLayout(g_uiEvent.lblBt, g_uiEvent.lblBtDec, g_uiEvent.lblBtUnit, UI_FONT_C_BT, UI_FONT_C_BTD, UI_FONT_C_UNIT, -4);
    }

    if (g_uiEvent.lblDevTime && g_uiSnap.devSec != g_uiEvent.lastDevSec) {
        g_uiEvent.lastDevSec = g_uiSnap.devSec;
        lv_label_set_text_fmt(g_uiEvent.lblDevTime, "%02lu:%02lu",
                              static_cast<unsigned long>(g_uiSnap.devSec / 60),
                              static_cast<unsigned long>(g_uiSnap.devSec % 60));
    }
    if (g_uiEvent.lblDevC && g_uiEvent.lblDtr) {
        int dc10  = static_cast<int>(lroundf(g_uiSnap.devTempDelta * 10.0f));
        int dtr10 = static_cast<int>(lroundf(g_uiSnap.devDtrPct * 10.0f));
        if (dc10 != g_uiEvent.lastDevC10) {
            g_uiEvent.lastDevC10 = dc10;
            lv_label_set_text_fmt(g_uiEvent.lblDevC, "+%d.%d°", abs(dc10) / 10, abs(dc10) % 10);
        }
        if (dtr10 != g_uiEvent.lastDtr10) {
            g_uiEvent.lastDtr10 = dtr10;
            lv_label_set_text_fmt(g_uiEvent.lblDtr, "%d.%d%%", abs(dtr10) / 10, abs(dtr10) % 10);
        }
    }

    for (int i = 0; i < 6; ++i) {
        bool marked = g_uiSnap.evMarked[i];
        uiToneBtnSetOn(g_uiEvent.btnEv[i], marked);
        if (g_uiEvent.lblEvTime[i]) {
            if (marked) {
                if (!g_uiEvent.lastMark[i] || g_uiSnap.evSec[i] != g_uiEvent.lastEvSec[i]) {
                    g_uiEvent.lastMark[i]  = true;
                    g_uiEvent.lastEvSec[i] = g_uiSnap.evSec[i];
                    lv_label_set_text_fmt(g_uiEvent.lblEvTime[i], "%02lu:%02lu",
                                          static_cast<unsigned long>(g_uiSnap.evSec[i] / 60),
                                          static_cast<unsigned long>(g_uiSnap.evSec[i] % 60));
                    lv_obj_set_style_text_color(g_uiEvent.lblEvTime[i], lv_color_hex(UI_HEX_TEXT), LV_PART_MAIN);
                }
            } else if (g_uiEvent.lastMark[i] || g_uiEvent.lastEvSec[i] != 0) {
                g_uiEvent.lastMark[i]  = false;
                g_uiEvent.lastEvSec[i] = 0;
                lv_label_set_text(g_uiEvent.lblEvTime[i], "--:--");
                lv_obj_set_style_text_color(g_uiEvent.lblEvTime[i], lv_color_hex(UI_HEX_TEXT_MUTED), LV_PART_MAIN);
            }
        }
    }

    // DR 5 s confirmation popup countdown
    uiConfirmTick(g_uiEvent.confirmDr);
}
