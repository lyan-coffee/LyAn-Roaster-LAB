/*
 * lib/RoasterArtisan.h - Artisan over WiFi: WebSocket server (port 81) (tier 5)
 *
 * Protocol, JSON key names, accepted commands and the events pushed to Artisan.
 * Requires the "WebSockets" library by Markus Sattler (Links2004/arduinoWebSockets).
 *
 * Notes:
 *   - Enable flag: ENABLE_ARTISAN_WS. HAS_MODULE_ARTISAN = 1 when the code is present.
 *   - artisanInit() registers artisanStop() with radioOnWifiStop() (lib/RoasterRadio.h): RoasterNet closes the WebSocket before WiFi stops
 *     without knowing Artisan exists. Call artisanInit() AFTER webInit() so Artisan closes before Web.
 *   - artisanTick() writes g_state.artisanLinked; UI / console read the link state there.
 *   - Artisan selection is in RoasterNet.h: netSelectArtisan(on). Console: artisan on | off.
 *   - The web pages (/artisan, /artisan.aset) are in lib/RoasterWebArtisan.h, attached via webAddNav / webAddRoutes.
 *
 * ----------------------------------------------------------------------------------------------------------------------------
 */
/*
 * Artisan module: WebSocket server so the Artisan software can control the roaster over WiFi.
 *
 * Requires: "WebSockets" by Markus Sattler v2.7.2 (Links2004/arduinoWebSockets).
 * No ArduinoJson: the small JSON reader below is enough for Artisan's packets.
 *
 * Protocol (from the Artisan source, src/artisanlib/wsport.py + comm.py):
 *     Artisan sends: {"command":"getData","id":<n>,"roasterID":0}
 *     Reply:         {"id":<n>,"data":{"BT":165.3,"BurnerVal":70,"FanVal":30,"Drum":1,"Cool":0,"PID":1,"SV":200.0}}
 *                    (id must equal the request id; BT is always °C, the .aset sets Mode=C)
 *     Pushed events: {"pushMessage":"startRoasting"} at Charge
 *                    {"pushMessage":"endRoasting"}   at Drop
 *                    {"pushMessage":"addEvent","data":{"event":"colorChangeEvent"}} for DE
 *                    (firstCrackBeginningEvent / secondCrackBeginningEvent for 1FC / 2FC)
 *
 * Control commands, two forms:
 *     form 1: {"BurnerVal":70} / {"FanVal":30} / {"Drum":1} / {"Cool":1} / {"PID":1}
 *     form 2: {"command":"setHeater","value":70}  (setFan, setSV, heaterUp/Down, airUp/Down,
 *             drumOn/Off, coolOn/Off, pidOn/Off, drum, cool)
 * Events go one way only: roaster -> Artisan (artisanPush).
 *
 * Runs only when Artisan is selected on the screen and WiFi is up.
 * HiBean (BLE) and Artisan (WiFi) exclude each other - see lib/RoasterRadio.h.
 * Disable with ENABLE_ARTISAN_WS 0 (RoasterSetting.h).
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"
#include "RoasterState.h"
#include "RoasterBatch.h"
#include "RoasterControl.h"
#include "RoasterRadio.h"
#include "RoasterNet.h"

#if ENABLE_WIFI && ENABLE_ARTISAN_WS
#define HAS_MODULE_ARTISAN 1

#include <WebSocketsServer.h>

static WebSocketsServer g_ws(ARTISAN_WS_PORT);
static bool    g_wsOn     = false;             // server running
static int     g_wsCli    = -1;                // connected client index (-1 = none)
static uint8_t g_wsPushed = 0;                 // bit: which event was already pushed to Artisan (avoids pushing again)

static inline bool artisanLinked() { return g_wsOn && g_wsCli >= 0; }

// V3: the self-test sets this pointer to COLLECT replies / pushes (one string per line) instead of sending over the WebSocket
static String *g_wsCapture = nullptr;
static void wsSendTo(uint8_t num, String &r) { if (g_wsCapture) { *g_wsCapture += r; *g_wsCapture += '\n'; return; } DLOG("[Artisan <-] %s\n", r.c_str()); g_ws.sendTXT(num, r); }
static void wsBroadcast(String &r)          { if (g_wsCapture) { *g_wsCapture += r; *g_wsCapture += '\n'; return; } DLOG("[Artisan <-] %s\n", r.c_str()); g_ws.broadcastTXT(r); }

// ---------------------------------------------------------------- JSON toi gian
static String jsonStr(const String &j, const char *key) {
    String pat = "\"";
    pat += key;
    pat += "\"";
    int i = j.indexOf(pat);
    if (i < 0) return String("");
    i = j.indexOf(':', i + pat.length());
    if (i < 0) return String("");
    i++;
    while (i < (int)j.length() && (j[i] == ' ' || j[i] == '"')) i++;
    int e = i;
    while (e < (int)j.length() && j[e] != '"' && j[e] != ',' && j[e] != '}') e++;
    return j.substring(i, e);
}
static long jsonInt(const String &j, const char *key, long dflt) {
    const String v = jsonStr(j, key);
    return v.length() ? v.toInt() : dflt;
}
static inline bool jsonHas(const String &j, const char *key) {
    String pat = "\"";
    pat += key;
    pat += "\"";
    return j.indexOf(pat) >= 0;
}

// ---------------- commands from Artisan
static void artisanHandle(uint8_t num, const String &msg) {
    const long id = jsonInt(msg, "id", -1);
    Snap s;
    roasterSnapshot(s);

    if (jsonStr(msg, "command") == "getData") {            // Artisan asks for data every sampling period
        String r = "{\"id\":";
        r += String(id);
        r += ",\"data\":{\"BT\":";
        r += String(s.tempOk ? s.temp : -1.0f, 1);
        r += ",\"BurnerVal\":"; r += String(s.heat);
        r += ",\"FanVal\":";    r += String(s.vent);
        r += ",\"Drum\":";      r += String(s.drum ? 1 : 0);
        r += ",\"Cool\":";      r += String(s.cool ? 1 : 0);
        r += ",\"PID\":";       r += String(s.pid ? 1 : 0);
        r += ",\"SV\":";        r += String(s.sp, 1);
        r += "}}";
        wsSendTo(num, r);
        return;
    }

    // Form 1: Artisan sends the value directly
    bool direct = false;
    if (jsonHas(msg, "BurnerVal")) { panelSetHeater((int)jsonInt(msg, "BurnerVal", 0)); direct = true; }
    if (jsonHas(msg, "FanVal"))    { panelSetAir((int)jsonInt(msg, "FanVal", 0));       direct = true; }
    if (jsonHas(msg, "Drum"))      { panelSetDrum(jsonInt(msg, "Drum", 0) != 0);        direct = true; }
    if (jsonHas(msg, "Cool"))      { panelSetCool(jsonInt(msg, "Cool", 0) != 0);        direct = true; }
    if (jsonHas(msg, "PID"))       { panelSetPID(jsonInt(msg, "PID", 0) != 0);          direct = true; }

    if (!direct) {                                          // Form 2: {"command":"...","value":n}
        const String cmd = jsonStr(msg, "command");
        const long v = jsonInt(msg, "value", -1);
        if      (cmd == "setHeater" && v >= 0) panelSetHeater((int)v);
        else if (cmd == "setFan"    && v >= 0) panelSetAir((int)v);
        else if (cmd == "setSV"     && v >= 0) panelSetSetpoint((float)v);
        else if (cmd == "heaterUp")            panelSetHeater(constrain(s.heat + ARTISAN_STEP, 0, 100));   // +5% button in Artisan
        else if (cmd == "heaterDown")          panelSetHeater(constrain(s.heat - ARTISAN_STEP, 0, 100));
        else if (cmd == "airUp")               panelSetAir(constrain(s.vent + ARTISAN_STEP, 0, 100));
        else if (cmd == "airDown")             panelSetAir(constrain(s.vent - ARTISAN_STEP, 0, 100));
        else if (cmd == "drumOn")              panelSetDrum(true);
        else if (cmd == "drumOff")             panelSetDrum(false);
        else if (cmd == "coolOn")              panelSetCool(true);
        else if (cmd == "coolOff")             panelSetCool(false);
        else if (cmd == "pidOn")               panelSetPID(true);
        else if (cmd == "pidOff")              panelSetPID(false);
        else if (cmd == "drum")                panelSetDrum(v != 0);
        else if (cmd == "cool")                panelSetCool(v != 0);
        // Start / stop / off / charge / dry / fcs / scs / drop commands from Artisan are NOT accepted.
        // Artisan event buttons only mark Artisan's graph; the roaster is not controlled back (no double marking).
        // Batch start / end and emergency stop are done on the roaster screen.
    }

    if (id >= 0) { String r = "{\"id\":"; r += String(id); r += ",\"data\":{\"ok\":1}}"; wsSendTo(num, r); }
}

static void wsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t len) {
    switch (type) {
        case WStype_CONNECTED:
            g_wsCli = (int)num;
            break;
        case WStype_DISCONNECTED:
            if ((int)num == g_wsCli) g_wsCli = -1;
            break;
        case WStype_TEXT: {
            String m;
            m.reserve(len + 1);
            for (size_t i = 0; i < len; i++) m += (char)payload[i];
            DLOG("[Artisan ->] %s\n", m.c_str());           // log convention: "->" = command sent to the roaster, "<-" = read from the roaster
            artisanHandle(num, m);
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------- day moc len Artisan
static void artisanPush(const Snap &s) {
    if (!s.live) { g_wsPushed = 0; return; }               // new batch: allow pushing again from the start
    if (g_wsCli < 0) return;
    static const struct { uint8_t ev; uint8_t bit; const char *msg; } M[] = {
        { EV_CHARGE, 0x01, "{\"pushMessage\":\"startRoasting\"}" },
        { EV_DE,     0x02, "{\"pushMessage\":\"addEvent\",\"data\":{\"event\":\"colorChangeEvent\"}}" },
        { EV_FC1,    0x04, "{\"pushMessage\":\"addEvent\",\"data\":{\"event\":\"firstCrackBeginningEvent\"}}" },
        { EV_FC2,    0x08, "{\"pushMessage\":\"addEvent\",\"data\":{\"event\":\"secondCrackBeginningEvent\"}}" },
        { EV_DROP,   0x10, "{\"pushMessage\":\"endRoasting\"}" },
    };
    for (const auto &m : M)
        if ((s.evDone & (1 << m.ev)) && !(g_wsPushed & m.bit)) {
            g_wsPushed |= m.bit;
            String t(m.msg);
            wsBroadcast(t);
        }
}

// ---------------- start / stop / loop
static void artisanStop() {
    if (!g_wsOn) return;
    g_ws.close();
    g_wsOn = false;
    g_wsCli = -1;
    g_wsPushed = 0;
    LOCK(); g_state.artisanLinked = false; UNLOCK();
}

// Called every 100 ms from the tick list (core 0), right after webTick. Non-blocking.
static void artisanTick() {
    // Only needs the RADIO on. Do NOT use netOnline(): during a roast a single WiFi.status() blip
    // makes netOnline() false and would close the WebSocket server, dropping Artisan mid-batch.
    // While the radio is on, keep the server running; the client reconnects by itself.
    // Also works when only the LyAn SoftAP is up (netOnline() is always false then).
    if (!artisanWanted || !netRadioOn()) { artisanStop(); return; }
    if (!g_wsOn) {
        g_ws.begin();
        g_ws.onEvent(wsEvent);
        g_wsOn = true;
        g_wsCli = -1;
    }
    g_ws.loop();
    Snap s;
    roasterSnapshot(s);
    artisanPush(s);
    const bool linked = artisanLinked();
    LOCK(); g_state.artisanLinked = linked; UNLOCK();
}

// Call once in setup(), AFTER webInit(): RoasterNet closes the WebSocket before WiFi stops (registered after Web, so it closes first)
static void artisanInit() {
    static bool done = false;
    if (done) return;
    done = true;
    radioOnWifiStop(artisanStop);
}

#else   // #else   // ------------------------------------------------- no Artisan

static inline void artisanTick() {}
static inline void artisanStop() {}
static inline void artisanInit() {}
static inline bool artisanLinked() { return false; }

#endif
