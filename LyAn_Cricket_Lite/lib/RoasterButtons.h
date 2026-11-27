/*
 * lib/CricketButtons.h - THREE BUTTONS: SHORT PRESS / LONG PRESS (item D of C0)                    (tier 1)
 *
 *   buttonsBegin()    set the pins (input with pull-up) and take the current level as the start state. Call once in setup().
 *   buttonsPoll()     debounce + detect. Call from loop() as often as possible (every ~10 ms). Never blocks.
 *   buttonPop(ev)     take the oldest event; false when none. Events: id BTN_BOOT / BTN_PWR / BTN_PLUS,
 *                     type BTN_SHORT (reported on release) or BTN_LONG (reported once, while still held, after BUTTON_LONG_MS).
 * Each event is also logged ("[BTN] PLUS short"). Events wait in a ring of BTN_QUEUE entries (oldest dropped when full),
 * guarded by LOCK(), so a consumer on the other core (UI, C2) can read them.
 * Buttons are active LOW. BOOT = GPIO 0 (held at reset it enters the bootloader). PWR: whether a long hold also cuts the
 * power in hardware is [UNKNOWN].
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"

#define HAS_MODULE_BUTTONS 1

enum { BTN_BOOT = 0, BTN_PWR = 1, BTN_PLUS = 2, BTN_N = 3 };
enum { BTN_SHORT = 1, BTN_LONG = 2 };
struct BtnEvent { uint8_t id; uint8_t type; };

// Optional hook for a button's LONG event: if set, it is called instead of queuing the event (Radio uses it for RADIO_SWITCH_BTN)
typedef void (*BtnLongHook)();
static BtnLongHook g_btnLongHook[3] = { nullptr, nullptr, nullptr };
static void buttonSetLongHook(uint8_t id, BtnLongHook fn) { if (id < 3) g_btnLongHook[id] = fn; }

#define BTN_QUEUE 8
static const uint8_t      k_btnPin[BTN_N]  = { BTN_BOOT_PIN, BTN_PWR_PIN, BTN_PLUS_PIN };
static const char *const  k_btnName[BTN_N] = { "BOOT", "PWR", "PLUS" };

struct BtnState { bool raw, stable, longDone; uint32_t tChange, tDown; };
static BtnState g_btn[BTN_N];
static BtnEvent g_btnQ[BTN_QUEUE];
static uint8_t  g_btnHead = 0, g_btnCount = 0;

static void btnPush(uint8_t id, uint8_t type, uint32_t heldMs) {
    (void)heldMs;                                        // used only by the log
    DLOGM("BTN", "%s %s (held %u ms)\n", k_btnName[id], type == BTN_LONG ? "LONG" : "short", (unsigned)heldMs);
    if (type == BTN_LONG && g_btnLongHook[id]) { g_btnLongHook[id](); return; }   // hooked: not queued
    LOCK();
    if (g_btnCount == BTN_QUEUE) { g_btnHead = (uint8_t)((g_btnHead + 1) % BTN_QUEUE); g_btnCount--; }   // full: drop the oldest
    g_btnQ[(g_btnHead + g_btnCount) % BTN_QUEUE] = { id, type };
    g_btnCount++;
    UNLOCK();
}

static bool buttonPop(BtnEvent &ev) {
    bool ok = false;
    LOCK();
    if (g_btnCount > 0) {
        ev = g_btnQ[g_btnHead];
        g_btnHead = (uint8_t)((g_btnHead + 1) % BTN_QUEUE);
        g_btnCount--;
        ok = true;
    }
    UNLOCK();
    return ok;
}

static void buttonsBegin() {
    const uint32_t now = millis();
    for (uint8_t i = 0; i < BTN_N; i++) {
        pinMode(k_btnPin[i], INPUT_PULLUP);
        const bool down = (digitalRead(k_btnPin[i]) == LOW);
        g_btn[i] = { down, down, true, now, now };       // a button already held at start does not fire an event
    }
    DLOGM("BTN", "ready: BOOT=GPIO%d PWR=GPIO%d PLUS=GPIO%d, long press >= %d ms\n", BTN_BOOT_PIN, BTN_PWR_PIN, BTN_PLUS_PIN, BUTTON_LONG_MS);
}

static void buttonsPoll() {
    const uint32_t now = millis();
    for (uint8_t i = 0; i < BTN_N; i++) {
        BtnState &s = g_btn[i];
        const bool raw = (digitalRead(k_btnPin[i]) == LOW);
        if (raw != s.raw) {
            s.raw = raw;
            s.tChange = now;                             // level changed: restart the debounce time
        } else if (raw != s.stable && (uint32_t)(now - s.tChange) >= (uint32_t)BUTTON_DEBOUNCE_MS) {
            s.stable = raw;                              // level held long enough: accept it
            if (raw) {
                s.tDown = now;
                s.longDone = false;
            } else if (!s.longDone) {
                btnPush(i, BTN_SHORT, (uint32_t)(now - s.tDown));
            }
        }
        if (s.stable && !s.longDone && (uint32_t)(now - s.tDown) >= (uint32_t)BUTTON_LONG_MS) {
            s.longDone = true;
            btnPush(i, BTN_LONG, (uint32_t)(now - s.tDown));
        }
    }
}
