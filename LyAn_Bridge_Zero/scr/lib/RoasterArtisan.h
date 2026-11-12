/*
 * GPL v3
 * Artisan qua WiFi: máy chủ WebSocket cổng 81.
 * Artisan hỏi:  {"command":"getData","id":n}
 * Máy trả lời: {"id":n,"data":{"BT":..,"BurnerVal":..,"FanVal":..,"Drum":..,"Cool":..,"PID":..,"SV":..}}
 * Lệnh nhận cả 2 dạng: {"BurnerVal":70} hoặc {"command":"setHeater","value":70}
 */
#pragma once
#include <Arduino.h>
#include <WebSocketsServer.h>
#include "../RoasterSetting.h"
#include "RoasterState.h"
#include "RoasterControl.h"
#include "RoasterRadio.h"
#include "RoasterNet.h"

static WebSocketsServer g_ws(ARTISAN_WS_PORT);
static bool    g_wsOn  = false;
static int     g_wsCli = -1;                   // -1 = chưa ai nối

// JSON tối giản
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

// Lệnh từ Artisan
static void artisanHandle(uint8_t num, const String &msg) {
    const long id = jsonInt(msg, "id", -1);
    Snap s;
    roasterSnapshot(s);

    if (jsonStr(msg, "command") == "getData") {
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
        g_ws.sendTXT(num, r);
        return;
    }

    // Dạng 1: {"BurnerVal":70}
    bool direct = false;
    if (jsonHas(msg, "BurnerVal")) { ctlSetHeater((int)jsonInt(msg, "BurnerVal", 0)); direct = true; }
    if (jsonHas(msg, "FanVal"))    { ctlSetAir((int)jsonInt(msg, "FanVal", 0));       direct = true; }
    if (jsonHas(msg, "Drum"))      { ctlSetDrum(jsonInt(msg, "Drum", 0) != 0);        direct = true; }
    if (jsonHas(msg, "Cool"))      { ctlSetCool(jsonInt(msg, "Cool", 0) != 0);        direct = true; }
    if (jsonHas(msg, "PID"))       { ctlSetPID(jsonInt(msg, "PID", 0) != 0);          direct = true; }

    if (!direct) {                                          // Dạng 2: {"command":"...","value":n}
        const String cmd = jsonStr(msg, "command");
        const long v = jsonInt(msg, "value", -1);
        if      (cmd == "setHeater" && v >= 0) ctlSetHeater((int)v);
        else if (cmd == "setFan"    && v >= 0) ctlSetAir((int)v);
        else if (cmd == "setSV"     && v >= 0) ctlSetSetpoint((float)v);
        else if (cmd == "heaterUp")            ctlSetHeater(constrain(s.heat + ARTISAN_STEP, 0, 100));
        else if (cmd == "heaterDown")          ctlSetHeater(constrain(s.heat - ARTISAN_STEP, 0, 100));
        else if (cmd == "airUp")               ctlSetAir(constrain(s.vent + ARTISAN_STEP, 0, 100));
        else if (cmd == "airDown")             ctlSetAir(constrain(s.vent - ARTISAN_STEP, 0, 100));
        else if (cmd == "drumOn")              ctlSetDrum(true);
        else if (cmd == "drumOff")             ctlSetDrum(false);
        else if (cmd == "coolOn")              ctlSetCool(true);
        else if (cmd == "coolOff")             ctlSetCool(false);
        else if (cmd == "pidOn")               ctlSetPID(true);
        else if (cmd == "pidOff")              ctlSetPID(false);
        else if (cmd == "drum")                ctlSetDrum(v != 0);
        else if (cmd == "cool")                ctlSetCool(v != 0);
    }

    if (id >= 0) { String r = "{\"id\":"; r += String(id); r += ",\"data\":{\"ok\":1}}"; g_ws.sendTXT(num, r); }
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
            DLOG("[Artisan ->] %s\n", m.c_str());
            artisanHandle(num, m);
            break;
        }
        default:
            break;
    }
}

// Bật / tắt / vòng lặp
static void artisanStop() {
    if (!g_wsOn) return;
    g_ws.close();
    g_wsOn = false;
    g_wsCli = -1;
    g_artisanLinked = false;
}

// Gọi mỗi 100 ms (core 0)
static void artisanTick() {
    if (!artisanWanted || !netRadioOn()) { artisanStop(); return; }
    if (!g_wsOn) {
        g_ws.begin();
        g_ws.onEvent(wsEvent);
        g_wsOn = true;
        g_wsCli = -1;
    }
    g_ws.loop();
    g_artisanLinked = (g_wsCli >= 0);
}
