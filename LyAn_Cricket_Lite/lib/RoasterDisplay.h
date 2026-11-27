/*
 * lib/CricketDisplay.h - ST7789 PANEL + BACKLIGHT (item B of C0, driver part)                      (tier 1)
 *
 *   displayBegin()            create bus + panel and start it. Call ONCE, from the LVGL task (not from setup()).
 *                             Returns false when the panel does not start; the backlight stays off.
 *   displayFlush(x,y,w,h,px)  send an RGB565 block (native 16-bit pixels) to the panel
 *   displayBacklight(on)      backlight on (at BACKLIGHT_PCT) / off. PWM on TFT_BLK (active HIGH [ASSUMPTION]); if PWM cannot start it is a plain on / off pin.
 *   displayBacklightPct(p)    set the 'on' level 0-100 % at run time (C7 / Settings will use it). Auto-dim logic itself belongs to C7.
 * No LVGL here: this file only knows the panel. Pins come from boards/.
 * Library: GFX_Library_for_Arduino (Arduino_GFX) 1.6.0. Panel drawn directly (no canvas): 240x240 is small and
 * LVGL already renders in partial blocks.
 */
#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"

#define HAS_MODULE_DISPLAY 1

static Arduino_GFX *g_gfx = nullptr;

static bool    g_blPwm = false;                          // true = backlight driven by PWM (LEDC)
static uint8_t g_blPct = BACKLIGHT_PCT;                  // backlight level when on, %
static bool    g_blOn = false;

static void displayBacklight(bool on) {
    g_blOn = on;
    if (g_blPwm) ledcWrite(TFT_BLK, on ? (uint32_t)(g_blPct * 255U / 100U) : 0U);
    else         digitalWrite(TFT_BLK, on ? HIGH : LOW);
}

static void displayBacklightPct(uint8_t pct) {           // change level at runtime (0-100 %)
    g_blPct = pct > 100 ? 100 : pct;
    if (g_blOn) displayBacklight(true);
}

static bool displayBegin() {
    g_blPwm = ledcAttach(TFT_BLK, BACKLIGHT_PWM_HZ, 8);   // 8-bit PWM; fall back to a plain pin on failure
    if (!g_blPwm) pinMode(TFT_BLK, OUTPUT);
    DLOGM("DISP", "backlight %s, level %u%%\n", g_blPwm ? "PWM" : "plain pin", (unsigned)g_blPct);
    displayBacklight(false);                              // dark until the first frame is drawn
    Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED /* MISO */);
    g_gfx = new Arduino_ST7789(bus, TFT_RST, 0 /* rotation */, true /* IPS */, SCREEN_W, SCREEN_H);
    if (!g_gfx->begin()) {
        DLOGM("DISP", "panel begin() FAILED\n");
        return false;
    }
    g_gfx->fillScreen(0x0000);
    DLOGM("DISP", "panel ST7789 %dx%d started\n", SCREEN_W, SCREEN_H);
    return true;
}

static void displayFlush(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *px) {
    if (g_gfx) g_gfx->draw16bitRGBBitmap((int16_t)x, (int16_t)y, (uint16_t *)px, (int16_t)w, (int16_t)h);
}
