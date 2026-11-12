/*
 * GPL v3
 * Đèn RGB (WS2812, GPIO 21) báo chế độ và trạng thái kết nối.
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterState.h"

static void rgbWrite(uint8_t r, uint8_t g, uint8_t b, bool force = false) {
    static int lr = -1, lg = -1, lb = -1;
    if (!force && r == lr && g == lg && b == lb) return;
    lr = r; lg = g; lb = b;
    const uint8_t R = (uint16_t)r * RGB_LED_BRIGHTNESS / 255;
    const uint8_t G = (uint16_t)g * RGB_LED_BRIGHTNESS / 255;
    const uint8_t B = (uint16_t)b * RGB_LED_BRIGHTNESS / 255;
#if RGB_LED_SWAP_RG
    neopixelWrite(RGB_LED_PIN, G, R, B);
#else
    neopixelWrite(RGB_LED_PIN, R, G, B);
#endif
}

static inline void rgbInit() { rgbWrite(0, 0, 0, true); }

// Gọi mỗi vòng loop()
static void rgbTick() {
    static uint8_t  lastMode      = 0xFF;
    static bool     lastConnected = false;
    static bool     lastDropped   = false;
    static uint32_t lastMs        = 0;
    static bool     phase         = true;

    const uint32_t now     = millis();
    const bool     dropped = tempSignalDropped();
    const uint8_t  wmode   = g_workMode;

    if (factoryArmed()) {                                  // chờ xác nhận Reset Factory: trắng chớp nhanh
        static uint32_t fMs = 0;
        static bool     fPh = false;
        if (now - fMs >= RGB_BLINK_FAST_MS) { fMs = now; fPh = !fPh; }
        const uint8_t v = fPh ? 255 : 0;
        rgbWrite(v, v, v);
        return;
    }

    if (g_updating) {                                      // đang nạp firmware: hồng chớp nhanh
        static uint32_t uMs = 0;
        static bool     uPh = false;
        if (now - uMs >= RGB_BLINK_FAST_MS) { uMs = now; uPh = !uPh; }
        rgbWrite(uPh ? 255 : 0, uPh ? 40 : 0, uPh ? 150 : 0);
        return;
    }

    bool connected = false;
    if (wmode == MODE_BLE_HIBEAN)       connected = g_hibeanLinked;
    else if (wmode == MODE_ARTISAN_WS)  connected = g_artisanLinked;
    else                                connected = g_webLinked;

    // Đổi chế độ / trạng thái: sáng ngay
    if (wmode != lastMode || connected != lastConnected || dropped != lastDropped) {
        lastMode      = wmode;
        lastConnected = connected;
        lastDropped   = dropped;
        phase         = true;
        lastMs        = now;
    }

    if (dropped) {
        if (now - lastMs >= RGB_BLINK_FAST_MS) { lastMs = now; phase = !phase; }
        rgbWrite(phase ? 200 : 0, 0, phase ? 255 : 0);     // mất tín hiệu nhiệt độ: tím chớp nhanh
        return;
    }

    if (connected) {
        phase = true;                                      // đã nối: sáng đứng
    } else {
        if (now - lastMs >= RGB_BLINK_MS) {                // chờ nối: chớp
            lastMs = now;
            phase = !phase;
        }
    }

    uint8_t r = 0, g = 0, b = 0;
    if (phase) {
        switch (wmode) {
            case MODE_BLE_HIBEAN:  g = 255; break;         // xanh lá
            case MODE_ARTISAN_WS:  b = 255; break;         // xanh dương
            case MODE_WIFI_CONFIG: r = 255; break;         // đỏ
            default:               g = 255; break;
        }
    }
    rgbWrite(r, g, b);
}
