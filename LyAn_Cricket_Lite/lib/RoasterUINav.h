/*
 * lib/RoasterUINav.h - UI Page Navigation State (Tier 1)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <Arduino.h>

enum UiPage : uint8_t {
    UI_PAGE_WELCOME = 0,
    UI_PAGE_HOME,
    UI_PAGE_CONTROL,
    UI_PAGE_EVENT,
    UI_PAGE_SETTING,
    UI_PAGE_WIFI,
    UI_PAGE_PID,
    UI_PAGE_SYSTEM
};

inline UiPage g_uiPage       = UI_PAGE_WELCOME;
inline bool   g_uiRebuildReq = false;

static inline void uiNavGo(UiPage page) {
    if (g_uiPage != page) {
        g_uiPage = page;
        g_uiRebuildReq = true;
    }
}

static inline void uiNavRequestRebuild() {
    g_uiRebuildReq = true;
}

static inline void uiNavSwipe(bool batchLive) {
    if (!batchLive) {
        if (g_uiPage == UI_PAGE_HOME)         uiNavGo(UI_PAGE_CONTROL);
        else if (g_uiPage == UI_PAGE_CONTROL) uiNavGo(UI_PAGE_HOME);
    } else {
        if (g_uiPage == UI_PAGE_CONTROL)      uiNavGo(UI_PAGE_EVENT);
        else if (g_uiPage == UI_PAGE_EVENT)   uiNavGo(UI_PAGE_CONTROL);
    }
}
