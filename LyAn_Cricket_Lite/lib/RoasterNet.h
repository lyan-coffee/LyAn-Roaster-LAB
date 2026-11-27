/*
 * lib/RoasterNet.h - Network: WiFi + saved networks in NVS + own access point (SoftAP) + NTP time + lyan.local name (tier 4)
 *
 * Differences from a single-file build:
 *   - bleStackUp, wifiRadioUp, ramLog() come from lib/RoasterRadio.h.
 *   - No forward declaration of webStop() / artisanStop(): radioRunWifiStopHooks() is used instead (each WiFi module registers its close function).
 *   - The mDNS port comes from WEB_SERVER_PORT.
 *   - HAS_MODULE_NET = 1 when ENABLE_WIFI = 1; with ENABLE_WIFI = 0 only empty functions remain at the end of the file.
 * Call netInit() once in setup() (reads NVS only, does NOT start the radio); netTick() every 100 ms from the tick list (lib/RoasterTick.h) on core 0.
 * ----------------------------------------------------------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"
#include "RoasterRadio.h"
#include "RoasterClock.h"

/*
 * Network module: WiFi + several saved networks in NVS + own access point (SoftAP) + NTP time + lyan.local name
 *
 *  - NEVER blocks: WiFi.begin() / scanning run in the background; netTick() is called every 100 ms from the tick list (core 0, same core as the UI).
 *    The roaster control loop on core 1 uses nothing in this file.
 *  - Saves up to WIFI_MAX_NETS networks (SSID + password) in NVS "lynet". At boot: scan, pick the strongest saved network.
 *  - ENABLE_WIFI 0: every function becomes empty and the WiFi library is not linked.
 *
 *  STARTUP ORDER (important; a wrong order once caused a dark screen):
 *    netInit() only reads NVS and does NOT start the radio. The WiFi driver takes ~60-100 KB of INTERNAL RAM; starting it in setup()
 *    competes with the UI asking for two LVGL draw buffers on core 0 -> the UI allocation fails -> black screen with no message.
 *    So the radio starts in netTick() only when:
 *      (1) the UI is up  (g_uiReady, set at the end of ui_task)
 *      (2) NET_BOOT_DELAY_MS has passed since boot
 *      (3) free internal RAM >= NET_MIN_HEAP
 *    WiFi never starts by itself at boot (no "on" flag in NVS) - only the saved network list is remembered.
 *    SoftAP does not start by itself either (AP+STA uses the most RAM) - only when the LyAn button is pressed.
 *
 *  WiFi ON DEMAND. Only ONE 2.4 GHz radio runs at a time (WiFi and BLE share the antenna):
 *    ON when:  Artisan selected (kept on all session) | Network page WiFi / LyAn pressed | profile edited via web | OTA upload
 *    OFF when: Artisan deselected | unused for WIFI_IDLE_OFF_MS | START pressed in Manual / HiBean mode
 *    Roasting with HiBean or Manual needs no WiFi: real time only needs one NTP sync, then the chip clock keeps running,
 *    so logs stay correct with WiFi off.
*/

#if ENABLE_WIFI
#define HAS_MODULE_NET 1

#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <time.h>

enum : uint8_t { NET_OFF = 0, NET_CONNECTING = 1, NET_ONLINE = 2, NET_AP_ONLY = 3 };
enum : uint8_t { NSCAN_IDLE = 0, NSCAN_RUN = 1, NSCAN_DONE = 2 };

struct NetSaved { char ssid[33]; char pass[65]; };

static NetSaved g_netList[WIFI_MAX_NETS];
static uint8_t  g_netCount = 0;
static uint8_t  g_netDef   = 0;                 // default network (preferred when several are in range)
static bool     g_netWant  = false;             // user turned WiFi on (default OFF - see the startup order note at the top)
static bool     g_netApWant = false;            // user turned the SoftAP on
static bool     g_netRadio = false;             // WiFi driver started (NOT started in setup, see netRadioStart)
static bool     g_netLowMem = false;            // not enough internal RAM to start WiFi
static bool     g_netHold  = false;             // something needs WiFi on (Artisan, OTA upload): no auto-off
static uint32_t g_netLastUse = 0;               // last WiFi use (web page, Network page), for auto-off
static uint8_t  g_netMode  = NET_OFF;
static bool     g_mdnsOk = false;
static int8_t   g_netTry = -1;                  // network being tried (index in g_netList)
static uint32_t g_netT0 = 0;                    // time the join attempt started
static uint32_t g_netNextTry = 0;               // next retry time
static uint8_t  g_scanState = NSCAN_IDLE;
static int      g_scanN = 0;

// ---------------------------------------------------------------- NVS
static void netSave() {
    Preferences p;
    if (!p.begin("lynet", false)) return;
    p.putUChar("n", g_netCount);            // saves only the network list; WiFi on/off is per session and not remembered across boots
    p.putUChar("def", g_netDef);
    char k[8];
    for (uint8_t i = 0; i < g_netCount; i++) {
        snprintf(k, sizeof k, "s%u", i); p.putString(k, g_netList[i].ssid);
        snprintf(k, sizeof k, "p%u", i); p.putString(k, g_netList[i].pass);
    }
    p.end();
}

static void netLoad() {
    Preferences p;
    if (!p.begin("lynet", true)) return;
    g_netWant = g_netApWant = false;                    // every boot: WiFi OFF (see the note at the top)
    g_netCount  = p.getUChar("n", 0);
    g_netDef    = p.getUChar("def", 0);
    if (g_netCount > WIFI_MAX_NETS) g_netCount = WIFI_MAX_NETS;
    if (g_netDef >= g_netCount) g_netDef = 0;
    char k[8];
    for (uint8_t i = 0; i < g_netCount; i++) {
        snprintf(k, sizeof k, "s%u", i); p.getString(k, g_netList[i].ssid, sizeof g_netList[i].ssid);
        snprintf(k, sizeof k, "p%u", i); p.getString(k, g_netList[i].pass, sizeof g_netList[i].pass);
    }
    p.end();
}

static inline bool    netOnline()    { return g_netMode == NET_ONLINE; }
static inline bool    netApOn()      { return g_netApWant && g_netRadio; }
static inline bool    netTimeOk()    { return clockIsValid(); }
static inline bool    netRadioOn()   { return g_netRadio; }
// Mark "a WiFi user is active" - called from the web and Network pages to restart the auto-off timer
static inline void    netTouch()     { g_netLastUse = millis(); }
// Keep WiFi on, no auto-off (Artisan connected, OTA upload)
static inline void    netHold(bool on) { g_netHold = on; netTouch(); }
// Free internal RAM (bytes) - WiFi can only allocate from this, not PSRAM
static inline uint32_t netFreeRam()  { return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
static inline uint8_t netSavedCount(){ return g_netCount; }
static inline const char *netSavedSSID(int i) { return (i >= 0 && i < g_netCount) ? g_netList[i].ssid : ""; }
static inline bool    netIsDefault(int i)     { return i == (int)g_netDef; }
static String netSSID() { return g_netMode == NET_ONLINE ? WiFi.SSID() : String(""); }
static String netIP() {
    if (g_netMode == NET_ONLINE) return WiFi.localIP().toString();
    if (g_netApWant)             return WiFi.softAPIP().toString();
    return String("");
}
// 0 = no signal, 1..4 = bars
static int netBars(int rssi) {
    if (rssi >= -55) return 4;
    if (rssi >= -67) return 3;
    if (rssi >= -78) return 2;
    if (rssi <  -90) return 0;
    return 1;
}

// ---------------- network scan (runs in background)
static void netScanStart() {
    if (!g_netRadio || g_scanState == NSCAN_RUN) return;      // radio not started: cannot scan
    // Scanning needs the STA interface. With only the LyAn access point on (WIFI_AP mode)
    // scanNetworks has no STA to scan with, so the list is ALWAYS empty (device and web page).
    // So enable STA before scanning (the access point stays up if it was on).
    if (!g_netWant) WiFi.mode(g_netApWant ? WIFI_AP_STA : WIFI_STA);
    WiFi.scanDelete();
    g_scanN = 0;
    netTouch();
    // async = true (non-blocking), no hidden networks, active scan, 120 ms per channel instead of the default 300 ms:
    // scan finishes in ~1.5 s instead of ~4 s -> a phone viewing the web page over the access point is dropped less often.
    WiFi.scanNetworks(true, false, false, 120);
    g_scanState = NSCAN_RUN;
}
static inline uint8_t netScanState() { return g_scanState; }
static inline int     netScanCount() { return g_scanN; }
static String  netScanSSID(int i)    { return WiFi.SSID(i); }
static int     netScanRSSI(int i)    { return WiFi.RSSI(i); }
static bool    netScanLocked(int i)  { return WiFi.encryptionType(i) != WIFI_AUTH_OPEN; }
// is a password already saved for this SSID
static int netFindSaved(const char *ssid) {
    for (uint8_t i = 0; i < g_netCount; i++) if (strcmp(g_netList[i].ssid, ssid) == 0) return i;
    return -1;
}

// ---------------- join / leave
static void netApply();                                   // re-apply the WiFi mode per g_netWant / g_netApWant
static bool netRadioStart();                              // start the WiFi driver (only when the UI is up and RAM is enough)

static void netTryConnect(int idx) {
    if (idx < 0 || idx >= g_netCount) return;
    g_netTry = idx;
    g_netT0 = millis();
    g_netMode = NET_CONNECTING;
    WiFi.begin(g_netList[idx].ssid, g_netList[idx].pass);   // non-blocking
}

// Pick the saved network with the strongest signal in the scan; prefer the default network when equal
static int netPickBest() {
    int best = -1, bestR = -1000;
    for (int i = 0; i < g_scanN; i++) {
        const int k = netFindSaved(WiFi.SSID(i).c_str());
        if (k < 0) continue;
        int r = WiFi.RSSI(i);
        if (k == (int)g_netDef) r += 6;                    // default network gets bonus points
        if (r > bestR) { bestR = r; best = k; }
    }
    return best;
}

// Add / update the password of a network, then join at once
static bool netJoin(const char *ssid, const char *pass) {
    if (!ssid || !ssid[0]) return false;
    if (!g_netRadio && !netRadioStart()) return false;
    int k = netFindSaved(ssid);
    if (k < 0) {
        if (g_netCount >= WIFI_MAX_NETS) {                 // full: drop the oldest network (not the default)
            const uint8_t drop = (g_netDef == 0) ? (uint8_t)1 : (uint8_t)0;
            for (uint8_t i = drop; i + 1 < g_netCount; i++) g_netList[i] = g_netList[i + 1];
            g_netCount--;
            if (g_netDef > drop) g_netDef--;
        }
        k = g_netCount++;
    }
    strncpy(g_netList[k].ssid, ssid, sizeof g_netList[k].ssid - 1); g_netList[k].ssid[sizeof g_netList[k].ssid - 1] = 0;
    strncpy(g_netList[k].pass, pass ? pass : "", sizeof g_netList[k].pass - 1); g_netList[k].pass[sizeof g_netList[k].pass - 1] = 0;
    g_netWant = true;
    netSave();
    netApply();
    netTryConnect(k);
    return true;
}

static void netForget(int i) {
    if (i < 0 || i >= g_netCount) return;
    const bool cur = (g_netMode == NET_ONLINE && strcmp(g_netList[i].ssid, WiFi.SSID().c_str()) == 0);
    for (uint8_t k = i; k + 1 < g_netCount; k++) g_netList[k] = g_netList[k + 1];
    g_netCount--;
    if (g_netDef >= g_netCount) g_netDef = 0;
    else if (g_netDef > i) g_netDef--;
    netSave();
    if (cur) { WiFi.disconnect(); g_netMode = NET_OFF; g_netNextTry = millis(); }
}

// ---------------------------------------------------------------- SoftAP + mDNS + NTP
static void netApply() {
    if (!g_netRadio) return;                                            // radio not started: do not touch the WiFi driver
    const bool sta = g_netWant;
    const bool ap  = g_netApWant;                                       // never start the AP by itself when no network is saved (AP+STA uses the most RAM)
    wifi_mode_t m = WIFI_OFF;
    if (sta && ap) m = WIFI_AP_STA;
    else if (sta)  m = WIFI_STA;
    else if (ap)   m = WIFI_AP;
    WiFi.mode(m);
    if (m == WIFI_OFF) { g_netMode = NET_OFF; g_mdnsOk = false; return; }
    WiFi.setSleep(true);                                                 // save power, reduces BLE interference
    if (ap) WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    if (!sta && ap) g_netMode = NET_AP_ONLY;
}

static void netTimeSync() { clockStartNtp(); }

static String netIsoTime()                 { return clockIsoTime(); }

// ---------------- radio on / off
// Call ONLY from netTick (core 0) or a UI button - NOT from setup(). See the note at the top.
static bool netRadioStart() {
    if (g_netRadio) return true;
    if (!g_uiReady) return false;                       // wait for the UI to allocate its 2 LVGL draw buffers
    if (millis() < NET_BOOT_DELAY_MS) return false;
    if (bleStackUp && !radioCoexistNow()) return false;  // BLE still up: wait for BLE to stop completely (the UI asks the user first). Auto mode (RADIO_COEXIST) allows both
    if (netFreeRam() < NET_MIN_HEAP) { g_netLowMem = true; return false; }
    g_netLowMem = false;
    WiFi.persistent(false);                             // managed in NVS "lynet"; WiFi must not save it itself
    WiFi.setAutoReconnect(true);
    WiFi.setHostname(WIFI_HOSTNAME);
    g_netRadio = true;
    wifiRadioUp = true;
    netTouch();
    netApply();
    g_netNextTry = 0;
    ramLog("sau netRadioStart");
    return true;
}

// Close the web server / DNS / OTA / WebSocket / mDNS BEFORE the radio goes off.
// Closing sockets after WiFi is off (webTick / artisanTick on the next 100 ms tick) means lwIP is already torn down
// -> device panics (reset). Most common when pressing "Disable Artisan" / turning WiFi off on the screen.
// This file does not know Web / Artisan exist (low tiers never call high tiers). Each WiFi module registers its own close function
// with radioOnWifiStop() (lib/RoasterRadio.h); radioRunWifiStopHooks() runs them in reverse order -> Artisan closes before Web.

static void netRadioStop() {
    if (!g_netRadio) return;
    radioRunWifiStopHooks();                            // v2.9.4: artisanStop(); webStop();
    if (g_mdnsOk) { MDNS.end(); g_mdnsOk = false; }
    WiFi.disconnect(true, true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    g_netRadio = false;
    wifiRadioUp = false;
    g_netHold = false;
    g_netMode = NET_OFF;
    g_mdnsOk = false;
    clockOnWifiDown();
    g_netTry = -1;
    g_scanState = NSCAN_IDLE;
    g_scanN = 0;
    ramLog("sau netRadioStop");
}

static void netSetEnabled(bool on) {
    if (g_netWant == on) return;
    g_netWant = on;
    netSave();
    if (on) { netTouch(); if (g_netRadio) netApply(); else netRadioStart(); g_netNextTry = 0; }   // try to join now
    else if (!g_netApWant) netRadioStop();                                            // turn the radio fully off: gives the RAM back
    else { WiFi.disconnect(true); g_netMode = NET_AP_ONLY; g_netTry = -1; g_mdnsOk = false; netApply(); }
}

static void netSetAp(bool on) {
    if (g_netApWant == on) return;
    g_netApWant = on;
    netSave();
    if (on) { netTouch(); if (g_netRadio) netApply(); else netRadioStart(); }
    else if (!g_netWant) netRadioStop();
    else { WiFi.softAPdisconnect(true); netApply(); }
}

// User SELECTS / DESELECTS Artisan (the UI and console share this function).
//   ON:  mark Artisan wanted (HiBean turns off), start WiFi and KEEP it on for the session.
//   OFF: back to auto-off. When the radio or AP goes off, netRadioStop() runs the registered close hooks (Artisan first), so artisanStop() need not be called.
// This file does not know the Artisan module exists: it only uses the artisanWanted flag in lib/RoasterRadio.h.
static void netSelectArtisan(bool on) {
    radioWantArtisan(on);
    if (on) { netSetEnabled(true); netHold(true); }
    else    { netHold(false); netSetEnabled(false); netSetAp(false); }
}

// ---------------- LINK MODE: only ONE of HiBean | Artisan | Web portal
// LINK_SOLO / LINK_HIBEAN: WiFi and access point fully off (BLE starts only after WiFi is down - see RoasterRadio.h).
// LINK_ARTISAN / LINK_WEB: WiFi on, join the saved network; none saved (or join fails) -> start the WIFI_AP_SSID access point. Kept on (no auto-off).
// save = true: store the mode in NVS ("lygia" / "lmode"). Web portal is NOT remembered (boot is always Standalone).
static void netLinkSave(uint8_t m) {
    Preferences p;
    if (p.begin("lygia", false)) { p.putUChar("lmode", m == LINK_WEB ? (uint8_t)LINK_SOLO : m); p.end(); }
}
static void netLinkMode(uint8_t m, bool save = true) {
    if (m >= LINK_COUNT) m = LINK_SOLO;
    if (m == LINK_ARTISAN || m == LINK_WEB) {
        if (m == LINK_ARTISAN) radioWantArtisan(true); else radioWantWeb(true);
        if (g_netCount > 0) netSetEnabled(true); else netSetEnabled(false);
        if (g_netCount == 0) netSetAp(true);
        netHold(true);
    } else {
        radioWantArtisan(false); radioWantWeb(false);
        netHold(false); netSetEnabled(false); netSetAp(false);        // turn WiFi off first (web / WebSocket close through the registered hooks)
        if (m == LINK_HIBEAN) radioWantHibean(true); else radioWantHibean(false);
    }
    if (save) netLinkSave(m);
}
// At boot (builds with a screen): load the saved mode. Never saved -> keep the default (HIBEAN_DEFAULT_ON). The radio starts late in netTick / bleService.
static void netLinkBoot() {
    Preferences p;
    if (!p.begin("lygia", false)) return;
    const bool has = p.isKey("lmode");
    const uint8_t m = has ? p.getUChar("lmode", LINK_SOLO) : 0;
    p.end();
    if (has) netLinkMode(m, false);
}
// Address to connect to (Artisan / Web portal): on a network -> network IP (or WIFI_HOSTNAME.local), on the access point -> AP IP; none -> empty
static String netAddr(bool hostName) {
    if (g_netMode == NET_ONLINE) return hostName ? String(WIFI_HOSTNAME) + ".local" : netIP();
    if (g_netApWant && g_netRadio) return WiFi.softAPIP().toString();
    return String();
}

// Reads NVS only. Does NOT start the radio here (see the note at the top).
static void netInit() {
    netLoad();
    g_netNextTry = 0;
}

// Called every 100 ms from the tick list (core 0). Non-blocking.
static void netTick() {
    const uint32_t now = millis();

    // late radio start: only when the UI is up and enough internal RAM is free (see the note at the top)
    if (g_netWant || g_netApWant) {
        if (!g_netRadio && !netRadioStart()) return;
    } else {
        if (g_netRadio) netRadioStop();
        return;
    }

    // auto-off when unused (no Artisan hold, no web / Network page open)
    if (!g_netHold && !artisanWanted && !webWanted && now - g_netLastUse > WIFI_IDLE_OFF_MS) {   // Artisan / Web portal never auto-off
        g_netWant = g_netApWant = false;
        netRadioStop();
        return;
    }

    // scan result
    if (g_scanState == NSCAN_RUN) {
        const int r = WiFi.scanComplete();
        if (r >= 0)      { g_scanN = r; g_scanState = NSCAN_DONE; }
        else if (r == WIFI_SCAN_FAILED) { g_scanN = 0; g_scanState = NSCAN_DONE; }
    }

    if (!g_netWant) return;

    const bool up = (WiFi.status() == WL_CONNECTED);
    if (up) {
        if (g_netMode != NET_ONLINE) {
            g_netMode = NET_ONLINE;
            g_netTry = -1;
            netTimeSync();
            if (!g_mdnsOk) g_mdnsOk = MDNS.begin(WIFI_HOSTNAME);
            if (g_mdnsOk) MDNS.addService("http", "tcp", WEB_SERVER_PORT);
        }
        return;
    }

    // not connected yet
    if (g_netMode == NET_CONNECTING) {
        if (now - g_netT0 < WIFI_CONNECT_TMO_MS) return;
        WiFi.disconnect();
        if (g_netHold && !g_netApWant) netSetAp(true);               // Artisan / Web portal cannot join a network -> start the access point
        g_netMode = g_netApWant ? NET_AP_ONLY : NET_OFF;
        g_netTry = -1;
        g_netNextTry = now + 5000;
        return;
    }
    if (g_netCount == 0) { g_netMode = g_netApWant ? NET_AP_ONLY : NET_OFF; return; }   // no saved network: press LyAn to configure
    if (now < g_netNextTry) return;

    // First attempt after turning on: join the default network directly, skipping the scan (saves 2-4 s so NTP time arrives sooner).
    // If that fails (WIFI_CONNECT_TMO_MS), later attempts go back to scan + strongest network.
    if (g_netNextTry == 0) {
        g_netNextTry = 1;                       // mark the direct attempt as used
        netTryConnect((int)g_netDef);
        return;
    }

    // retry: scan first to pick the strongest network
    if (g_scanState == NSCAN_IDLE) { netScanStart(); return; }
    if (g_scanState == NSCAN_RUN)  return;
    const int best = netPickBest();
    g_scanState = NSCAN_IDLE;
    if (best >= 0) netTryConnect(best);
    else {
        if (g_netHold && !g_netApWant) netSetAp(true);               // no saved network found -> start the access point
        g_netMode = g_netApWant ? NET_AP_ONLY : NET_OFF; g_netNextTry = now + WIFI_RETRY_MS;
    }
}

#else   // ------------------------------------------------- ENABLE_WIFI 0
#include <Preferences.h>

enum : uint8_t { NET_OFF = 0, NET_CONNECTING = 1, NET_ONLINE = 2, NET_AP_ONLY = 3 };
static inline void netInit() {}
static inline void netTick() {}
static inline void netSelectArtisan(bool on) { radioWantArtisan(on); }
static inline void netLinkSave(uint8_t) {}
static void netLinkBoot() {
    Preferences p;
    if (!p.begin("lygia", false)) return;
    const bool has = p.isKey("lmode");
    const uint8_t m = has ? p.getUChar("lmode", LINK_SOLO) : 0;
    p.end();
    if (has) radioWantHibean(m == LINK_HIBEAN);
}
static inline void netLinkMode(uint8_t m, bool = true) { radioWantArtisan(false); radioWantWeb(false); radioWantHibean(m == LINK_HIBEAN); }
static inline String netAddr(bool) { return String(); }
static inline bool netOnline()  { return false; }
static inline bool netTimeOk()  { return clockIsValid(); }
static inline String netIsoTime() { return clockIsoTime(); }

#endif  // ENABLE_WIFI
