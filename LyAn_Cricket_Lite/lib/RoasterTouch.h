/*
 * lib/CricketTouch.h - CST816 CAPACITIVE TOUCH (item C of C0)                                       (tier 1)
 *
 *   touchBegin()          start the I2C bus (Wire on TOUCH_SDA / TOUCH_SCL), reset the chip, scan the bus and log
 *                         what answers (so the real I2C address is visible), disable auto-sleep. Call ONCE from setup().
 *                         Returns true when TOUCH_ADDR answers.
 *   touchRead(x, y)       read the chip NOW (I2C). true = a finger is down; x, y in screen pixels (0..SCREEN_W-1).
 *                         Logs "down" / "up" edges. Call from ONE thread only (the LVGL task, or touchTick()).
 *   touchLast(x, y)       last point seen by touchRead() (no I2C). Returns true if the finger was down.
 *   touchTick()           for builds WITHOUT a display: registered with tickAdd(), it reads and logs. With a display,
 *                         lib/CricketUI.h reads the touch from its LVGL input callback instead.
 * The I2C bus is shared with the audio codec (C3): C3 must reuse this Wire, not call Wire.begin() with other pins.
 * The INT pin is not used: the chip is polled (every ~33 ms by LVGL).
 * [ASSUMPTION] CST816 register map as in its datasheet: 0x01 gesture, 0x02 finger count, 0x03..0x06 X/Y; 0xFE = disable auto-sleep.
 */
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"

#define HAS_MODULE_TOUCH 1

#ifndef TOUCH_PRESS_CONFIRM
  #define TOUCH_PRESS_CONFIRM   2     // consecutive reads with a finger before a PRESS is accepted (~20 ms @10 ms): filters ghost touches
#endif
#ifndef TOUCH_RELEASE_CONFIRM
  #define TOUCH_RELEASE_CONFIRM 3     // consecutive reads without a finger before a RELEASE is accepted (~30 ms): debounce
#endif

static bool     g_touchDown = false;
static uint8_t  g_touchDnCnt = 0, g_touchUpCnt = 0;
static uint16_t g_touchX = 0, g_touchY = 0;

static bool touchBegin() {
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(TOUCH_I2C_HZ);

    pinMode(TOUCH_RST, OUTPUT);                         // hardware reset pulse (short, setup() only)
    digitalWrite(TOUCH_RST, LOW);  delay(10);
    digitalWrite(TOUCH_RST, HIGH); delay(50);

    // Scan the shared bus once and log every address that answers
    char list[96];
    size_t n = 0;
    list[0] = 0;
    bool found = false;
    for (uint8_t a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            if (a == TOUCH_ADDR) found = true;
            if (n + 6 < sizeof(list)) n += (size_t)snprintf(list + n, sizeof(list) - n, " 0x%02X", a);
        }
    }
    DLOGM("TOUCH", "I2C scan (SDA %d, SCL %d):%s\n", TOUCH_SDA, TOUCH_SCL, n ? list : " nothing answered");
    DLOGM("TOUCH", "CST816 at 0x%02X: %s\n", TOUCH_ADDR, found ? "FOUND" : "NOT FOUND (check TOUCH_ADDR / pins)");

    if (found) {                                        // keep the chip awake so it always answers
        Wire.beginTransmission(TOUCH_ADDR);
        Wire.write((uint8_t)0xFE);
        Wire.write((uint8_t)0x01);
        const uint8_t err = Wire.endTransmission();
        (void)err;                                      // used only by the log
        DLOGM("TOUCH", "auto-sleep off: %s\n", err == 0 ? "ok" : "write failed");
    }
    return found;
}

static bool touchRead(uint16_t &x, uint16_t &y) {
    bool raw = false;                                   // raw chip state (unfiltered)
    uint16_t rx = 0, ry = 0;

    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write((uint8_t)0x01);
    if (Wire.endTransmission() == 0 && Wire.requestFrom(TOUCH_ADDR, 6) == 6) {
        uint8_t d[6];
        for (int i = 0; i < 6; i++) d[i] = (uint8_t)Wire.read();
        if (d[1] != 0 && d[1] <= 2) {                   // finger count 1..2
            rx = (uint16_t)(((d[2] & 0x0F) << 8) | d[3]);
            ry = (uint16_t)(((d[4] & 0x0F) << 8) | d[5]);
            raw = true;
        }
    }

    if (raw) {
#if TOUCH_SWAP_XY
        { const uint16_t t = rx; rx = ry; ry = t; }
#endif
        if (rx >= SCREEN_W) rx = SCREEN_W - 1;
        if (ry >= SCREEN_H) ry = SCREEN_H - 1;
#if TOUCH_INVERT_X
        rx = SCREEN_W - 1 - rx;
#endif
#if TOUCH_INVERT_Y
        ry = SCREEN_H - 1 - ry;
#endif
    }

    // Debounce: a press needs TOUCH_PRESS_CONFIRM consecutive samples, a release TOUCH_RELEASE_CONFIRM.
    // While a release is pending, keep the last touch point (LVGL never sees release-then-press: no double or false taps).
    bool down;
    if (raw) {
        g_touchUpCnt = 0;
        if (g_touchDnCnt < 255) g_touchDnCnt++;
        down = g_touchDown || (g_touchDnCnt >= TOUCH_PRESS_CONFIRM);
    } else {
        g_touchDnCnt = 0;
        if (g_touchUpCnt < 255) g_touchUpCnt++;
        down = g_touchDown && (g_touchUpCnt < TOUCH_RELEASE_CONFIRM);
    }
    if (down && !raw) { rx = g_touchX; ry = g_touchY; }

    // Edge log (and, with TOUCH_DEBUG, a point log at most every 100 ms while the finger moves)
    if (down && !g_touchDown) DLOGM("TOUCH", "down x=%u y=%u\n", (unsigned)rx, (unsigned)ry);
    if (!down && g_touchDown) DLOGM("TOUCH", "up   x=%u y=%u\n", (unsigned)g_touchX, (unsigned)g_touchY);
#if TOUCH_DEBUG
    if (down && g_touchDown && (rx != g_touchX || ry != g_touchY)) {
        static uint32_t lastLog = 0;
        const uint32_t now = millis();
        if ((uint32_t)(now - lastLog) >= 100) { lastLog = now; DLOGM("TOUCH", "move x=%u y=%u\n", (unsigned)rx, (unsigned)ry); }
    }
#endif
    if (down) { g_touchX = rx; g_touchY = ry; }
    g_touchDown = down;
    x = g_touchX;
    y = g_touchY;
    return down;
}
