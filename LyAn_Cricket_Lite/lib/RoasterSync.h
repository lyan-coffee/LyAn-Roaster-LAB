/*
 * lib/RoasterSync.h - Thread sync, virtual clock, debug log (tier 0)
 *
 * UI (LVGL) runs on core 0, the roaster protocol on core 1 (loop), BLE in its own task.
 * All access to machine state goes through this recursive mutex: LOCK() ... UNLOCK().
 */
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../RoasterSetting.h"

static SemaphoreHandle_t g_mux = nullptr;

inline void syncInit() {
    if (!g_mux) g_mux = xSemaphoreCreateRecursiveMutex();
}

// Safe version: before syncInit(), LOCK/UNLOCK do nothing (no deadlock)
#define LOCK()   do { if (g_mux) xSemaphoreTakeRecursive(g_mux, portMAX_DELAY); } while (0)
#define UNLOCK() do { if (g_mux) xSemaphoreGiveRecursive(g_mux); } while (0)

// Last command received from HiBean (ms)
static volatile uint32_t g_lastHiBeanMs = 0;

// UI is built (draw buffers allocated, backlight on). Set at the end of ui_task.
// WiFi waits for this flag before enabling the radio; starting earlier competes with the UI for internal RAM (dark screen).
static volatile bool g_uiReady = false;
// Splash screen showing. loop() waits for it to clear before mounting FFat (flash writes during the animation can glitch the display).
static volatile bool g_uiSplash = false;

// -----------------------------------------------------------------------------
// VIRTUAL CLOCK bclock()
// The roast clock (Time, graph, events, RoR) follows the temperature source's time. Normally = millis().
// In simulation (g_timeScale = 7) it runs 7x faster, in step with the simulated temperature,
// so Time, graph and events match. Confirmation timers (Drop / END 5 s, hints) still use real millis().
// Call bclock() BEFORE every g_timeScale change.
// -----------------------------------------------------------------------------
static volatile float g_timeScale = 1.0f;     // time factor (1 = real; the simulator sets > 1). RoR is divided by it

inline uint32_t bclock() {
    static uint32_t last = 0, v = 0;
    LOCK();
    const uint32_t m = millis();
    v += (uint32_t)((float)(m - last) * g_timeScale + 0.5f);
    last = m;
    const uint32_t r = v;
    UNLOCK();
    return r;
}

// -----------------------------------------------------------------------------
// Debug log to Serial (only when SERIAL_DEBUG = 1 in RoasterSetting.h)
//   DLOG(...)            printf style
//   D_print / D_println  Serial.print style
// -----------------------------------------------------------------------------
#if SERIAL_DEBUG
  #define DLOG(...)            Serial.printf(__VA_ARGS__)
  #define DLOGM(tag, fmt, ...) Serial.printf("[" tag "] " fmt, ##__VA_ARGS__)
  #define D_print(...)         Serial.print(__VA_ARGS__)
  #define D_println(...)       Serial.println(__VA_ARGS__)
#else
  #define DLOG(...)            do {} while (0)
  #define DLOGM(tag, fmt, ...) do {} while (0)
  #define D_print(...)         do {} while (0)
  #define D_println(...)       do {} while (0)
#endif
