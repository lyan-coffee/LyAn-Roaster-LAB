/*
 * GPL v3
 * Nút BOOT và cờ bật / tắt BLE, Artisan, Web.
 * BLE và WiFi dùng chung một ăng-ten nên mỗi lúc chỉ bật một chế độ.
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterState.h"

static volatile bool bleStackUp    = false;   // BLE đang chạy
static volatile bool wifiRadioUp   = false;   // WiFi đang bật
static volatile bool hibeanWanted  = true;    // muốn bật BLE
static volatile bool artisanWanted = false;   // muốn bật Artisan
static volatile bool webWanted     = false;   // muốn bật web

static inline const char *workModeName(uint8_t m) {
    switch (m) {
        case MODE_BLE_HIBEAN:  return "BLE_HIBEAN (Green)";
        case MODE_ARTISAN_WS:  return "ARTISAN_WS (Blue)";
        case MODE_WIFI_CONFIG: return "WIFI_WEB_CONFIG (Red)";
        default:               return "?";
    }
}

static inline void bootButtonInit() {
    pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
}

// Trả về true khi vừa nhấn nút BOOT (có chống dội)
static inline bool bootButtonPressed() {
    static bool     lastRaw     = true;
    static bool     stableState = true;
    static uint32_t lastChange  = 0;
    const uint32_t  now         = millis();
    const bool      raw         = (digitalRead(BOOT_BTN_PIN) == HIGH);

    if (raw != lastRaw) {
        lastRaw = raw;
        lastChange = now;
    }
    if ((now - lastChange) >= BOOT_BTN_DEBOUNCE_MS && raw != stableState) {
        stableState = raw;
        if (!stableState) {
            return true;
        }
    }
    return false;
}
