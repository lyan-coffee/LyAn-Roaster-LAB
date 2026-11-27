/*
 * lib/RoasterRadio.h - Shared antenna: BLE and WiFi are never on at the same time (tier 3)
 *
 * The ESP32-S3 has ONE 2.4 GHz antenna and limited internal RAM, so Bluetooth (HiBean) and WiFi (Web, Artisan, OTA) take turns.
 * This file only holds the status flags both sides read; it does not know what BLE or Net actually do:
 *
 *   bleStackUp   NimBLE is initialized  -> RoasterNet does NOT enable WiFi while set
 *   wifiRadioUp  WiFi driver is on      -> BLE module does NOT initialize BLE while set
 *   ramLog(tag)  prints free internal RAM (SERIAL_DEBUG only), around radio on/off to track leaks
 *
 *
 * WiFi stop hooks (close before WiFi goes off):
 *   The web server, DNS and Artisan WebSocket must close BEFORE WiFi is stopped; otherwise lwIP is torn down with sockets open
 *   and the device panics (reset). Each WiFi module registers its own close function (radioOnWifiStop);
 *   RoasterNet only calls radioRunWifiStopHooks().
 *   Hooks run in REVERSE registration order (registered later = closed first): Artisan (registered after Web) closes before Web.
 *
 * The user CHOOSES HiBean (Bluetooth) or Artisan (WiFi); they exclude each other:
 *   hibeanWanted   HiBean (BLE) wanted.    artisanWanted  Artisan (WiFi) wanted.
 *   radioWantHibean(on) / radioWantArtisan(on)  change the choice; enabling one disables the other.
 *   radioHibeanText()  one-line HiBean status text (console, [HB] beat).
 * Actual on/off happens in the BLE module (bleService) for Bluetooth and RoasterNet for WiFi. This file only holds flags.
 *
 * Only ONE of three: HiBean (BLE) | Artisan (WebSocket 81) | Web portal (http 80); see webWanted and radioLinkMode() (LINK_*).
 *
 * Screenless boards: RADIO_COEXIST = 1 + auto mode lets BLE and WiFi run together when idle (radioAuto, radioAutoDecide). Off by default.
 *
 * Tier 3: includes lower tiers only (RoasterSetting.h, RoasterSync.h, RoasterState.h).
 */
#pragma once
#include <Arduino.h>
#include <esp_heap_caps.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"
#include "RoasterState.h"

#define HAS_MODULE_RADIO 1

static volatile bool bleStackUp  = false;
static volatile bool wifiRadioUp = false;

// ---------------- HiBean / Artisan choice (mutually exclusive)
static volatile bool hibeanWanted  = (HIBEAN_DEFAULT_ON && ENABLE_HIBEAN_BLE);
static volatile bool artisanWanted = false;

// ---------------- auto radio (only when RADIO_COEXIST = 1; see RoasterSetting.h)
// radioAuto = true: BLE and WiFi may run together, both wanted flags may be set; the .ino decides who is on / off.
// radioAuto = false: mutually exclusive. Builds with a screen (RADIO_COEXIST = 0) always use this.
static volatile bool radioAuto     = (RADIO_COEXIST && RADIO_AUTO_DEFAULT);
static inline bool radioCoexistNow() { return RADIO_COEXIST && radioAuto; }

// Web portal (http port 80) is the third choice and excludes HiBean and Artisan (with a screen: only ONE of three runs).
static volatile bool webWanted = false;

static void radioWantHibean(bool on)  { hibeanWanted = on;  if (on && !radioCoexistNow()) { artisanWanted = false; webWanted = false; } }
static void radioWantArtisan(bool on) { artisanWanted = on; if (on && !radioCoexistNow()) { hibeanWanted = false; webWanted = false; } }
static void radioWantWeb(bool on)     { webWanted = on;     if (on) { hibeanWanted = false; artisanWanted = false; } }

// ---------------- LINK MODE ("Mode" button on the main page)
//   LINK_SOLO     standalone roasting: Bluetooth and WiFi both off
//   LINK_HIBEAN   HiBean qua Bluetooth
//   LINK_ARTISAN  Artisan over WiFi (WebSocket port 81)
//   LINK_WEB      Web portal over WiFi (http port 80)
// Real on/off is in netLinkMode() (lib/RoasterNet.h). Here the current mode is only derived from the three flags.
enum : uint8_t { LINK_SOLO = 0, LINK_HIBEAN = 1, LINK_ARTISAN = 2, LINK_WEB = 3, LINK_COUNT = 4 };
static inline uint8_t radioLinkMode() { return hibeanWanted ? LINK_HIBEAN : artisanWanted ? LINK_ARTISAN : webWanted ? LINK_WEB : LINK_SOLO; }

static void ramLog(const char *tag) {
    (void)tag;                                   // SERIAL_DEBUG = 0: D_print is empty
    D_print("RAM "); D_print(tag);
    D_print(": free="); D_print((uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    D_print(" maxblk="); D_println((uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}

// ---------------- WiFi stop hooks
typedef void (*RadioStopFn)();
#define RADIO_STOP_HOOK_MAX 6
static RadioStopFn g_radioStopHooks[RADIO_STOP_HOOK_MAX];
static uint8_t     g_radioStopHookN = 0;

// Register a close function (call once in setup()). Returns false if already registered or full.
static bool radioOnWifiStop(RadioStopFn fn) {
    if (!fn) return false;
    for (uint8_t i = 0; i < g_radioStopHookN; i++) if (g_radioStopHooks[i] == fn) return false;
    if (g_radioStopHookN >= RADIO_STOP_HOOK_MAX) return false;
    g_radioStopHooks[g_radioStopHookN++] = fn;
    return true;
}

// Called by RoasterNet in netRadioStop(), BEFORE WiFi is stopped
static void radioRunWifiStopHooks() {
    for (int i = (int)g_radioStopHookN - 1; i >= 0; i--) g_radioStopHooks[i]();
}
