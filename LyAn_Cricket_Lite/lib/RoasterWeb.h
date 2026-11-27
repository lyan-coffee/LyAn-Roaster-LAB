/*
 * lib/RoasterWeb.h - Built-in Web Portal Core (HTTP:80, Captive DNS, PIN gate, Wi-Fi, About) (Tier 5)
 *
 * Runs on Core 0 (webTick). Automatically shuts down when a roast batch starts to free RAM.
 * Extra pages (such as Artisan .aset download) attach via webAddNav / webAddFoot / webAddRoutes.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"
#include "RoasterState.h"
#include "RoasterRadio.h"
#include "RoasterClock.h"
#include "RoasterNet.h"

#if (ENABLE_WIFI && ENABLE_WEB_PORTAL)
#define HAS_MODULE_WEB 1

#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <vector>

static WebServer  g_web(WEB_SERVER_PORT);
static DNSServer  g_dns;                        // resolve every domain name to 192.168.4.1 while on the own access point
static bool       g_webOn = false, g_dnsOn = false;
// True while a roast batch is active (shuts down Web Portal to free RAM).
static bool webRoastBusy() { return g_state.roastBusy; }

#include "RoasterWebI18n.h"          // wt(), wl(), WT[] (multi-language EN | VI | 中文) - needs g_web above

// ---------------- shared frame (look matches the device idle screen)
// Background gradient IDLE_BG_TOP -> IDLE_BG_BOT, cream text, gold accent #BD9940 (same colors as the idle-screen logo).
// Default CSS: SINGLE SOURCE. Embedded into every page and also the content of /default/www/style.css (RoasterWebFiles.h).
// If /www/style.css exists on FFat, the page loads it AFTER this CSS -> it only needs the rules to override.
static const char WEB_CSS_TXT[] PROGMEM =
  ":root{--g:#BD9940;--cr:#F3E5D0;--m:#9C8C7C}\n"
  "*{box-sizing:border-box}\n"
  "body{margin:0;min-height:100vh;padding:22px 16px 28px;"
  "background:linear-gradient(180deg,#0F1A28,#070B11);background-attachment:fixed;color:var(--cr);"
  "font:16px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}\n"
  ".w{max-width:430px;margin:0 auto}\n"
  ".lg{display:block;margin:4px auto 20px;width:188px;height:188px}\n"
  ".bt{width:100%;border-collapse:collapse;margin-top:10px;font-size:14px}\n"
  ".bt td{padding:6px 4px;border-top:1px solid #1d2838;vertical-align:top}\n"
  ".bt td:first-child{color:var(--g);white-space:nowrap;padding-right:10px}\n"
  ".bt code{font-size:12px;word-break:break-all}\n"
  "h1{font-size:15px;letter-spacing:.16em;text-align:center;color:var(--g);margin:0 0 22px;font-weight:700}\n"
  "h2{font-size:17px;color:var(--g);margin:0 0 14px;font-weight:600}\n"
  "a{color:var(--g);text-decoration:none}\n"
  "h2 a{color:inherit}\n"
  ".b{display:block;width:100%;padding:17px;margin:11px 0;border:1px solid #2d3a4a;border-radius:14px;"
  "background:rgba(189,153,64,.07);color:var(--cr);font-size:17px;font-weight:600;text-align:center;cursor:pointer}\n"
  ".b:active{background:rgba(189,153,64,.2)}.b.p{background:var(--g);color:#0B1018;border-color:var(--g)}\n"
  ".c{background:rgba(255,255,255,.04);border:1px solid #243041;border-radius:14px;padding:15px;margin:12px 0}\n"
  "input,select{width:100%;background:#0B121C;color:var(--cr);border:1px solid #2d3a4a;border-radius:10px;"
  "padding:12px;margin:7px 0;font-size:16px}\n"
  // select: remove the browser's default arrow (differs per device, off-center), use a thin gray arrow, vertically centered, 16 px from the right edge
  "select{-webkit-appearance:none;-moz-appearance:none;appearance:none;padding-right:44px;cursor:pointer;text-overflow:ellipsis;"
  "background-image:url(\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='14' height='8' viewBox='0 0 14 8'%3E%3Cpath d='M1 1l6 6 6-6' "
  "fill='none' stroke='%239C8C7C' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'/%3E%3C/svg%3E\");"
  "background-repeat:no-repeat;background-position:right 16px center;background-size:14px 8px}\n"
  "select:focus{outline:none;border-color:var(--g)}select option{background:#0B121C;color:var(--cr)}\n"
  "table{width:100%;border-collapse:collapse}td{padding:7px 0;border-bottom:1px solid #1b2430;vertical-align:top}\n"
  "td+td{text-align:right}.m{color:var(--m)}.s{font-size:15px}.g{color:#43D17A}.r{color:#ff6b6b}\n"
  ".hn{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:11px 0}.hn .b{margin:0;padding:11px 8px;font-size:16px}\n"
  ".f{text-align:center;margin-top:26px}.f a{margin:0 9px;font-size:15px;color:var(--m)}\n"
  ".lb{text-align:center;margin:-10px 0 16px;font-size:15px}.lb a{margin:0 9px;color:var(--m)}.lb b{margin:0 9px;color:var(--g)}\n";

#include "RoasterWebLogo.h"      // WEB_LOGO_SVGZ (gzip-compressed SVG logo)

// ---------------- EXTENSION: other modules attach pages / buttons to the web WITHOUT editing this file
// Call these in setup() (before the web starts). Key / path strings must live for the whole run (string literals).
#define WEB_NAV_MAX    6
#define WEB_FOOT_MAX   4
#define WEB_ROUTE_MAX  8
#define WEB_NEXT_MAX   8
struct WebLink { const char *key; const char *href; };
static WebLink  g_webNav[WEB_NAV_MAX];     static uint8_t g_webNavN = 0;
static WebLink  g_webFoot[WEB_FOOT_MAX];   static uint8_t g_webFootN = 0;
static void   (*g_webRoutes[WEB_ROUTE_MAX])(); static uint8_t g_webRoutesN = 0;
static const char *g_webNext[WEB_NEXT_MAX]; static uint8_t g_webNextN = 0;
static const char *g_webNextDef = "/";           // after the PIN, go to Home if "next" is invalid

static void webAddNav(const char *key, const char *href)   { if (g_webNavN  < WEB_NAV_MAX)  g_webNav[g_webNavN++]   = { key, href }; }
static void webAddRoutes(void (*fn)())                     { if (fn && g_webRoutesN < WEB_ROUTE_MAX) g_webRoutes[g_webRoutesN++] = fn; }
static bool webNextOk(const String &n) {
    if (n == "/wifi") return true;
    for (uint8_t i = 0; i < g_webNextN; i++) if (n == g_webNext[i]) return true;
    return false;
}

// ---------------- page frame + language choice
static String webEnc(const String &s) {                       // URL-encode for the query part
    String o;
    o.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '/') o += (char)c;
        else { char t[4]; snprintf(t, sizeof t, "%%%02X", c); o += t; }
    }
    return o;
}
// "EN | VI | 中文" under the title: tap -> same page with ?l=xx (other parameters kept). POST pages have no such bar.
static String webLangBar() {
    if (g_web.method() != HTTP_GET) return String();
    String base = g_web.uri() + "?";
    for (int i = 0; i < g_web.args(); i++) {
        if (g_web.argName(i) == "l") continue;
        base += webEnc(g_web.argName(i)) + "=" + webEnc(g_web.arg(i)) + "&";
    }
    static const char *const NM[3] = { "EN", "VI", "中文" };
    const int cur = wl();
    String s = F("<div class=lb>");
    for (int i = 0; i < 3; i++) {
        if (i) s += F("<span class=m>|</span>");
        if (i == cur) s += String("<b>") + NM[i] + "</b>";
        else s += "<a href='" + base + "l=" + wlCode(i) + "'>" + NM[i] + "</a>";
    }
    s += F("</div>");
    return s;
}

// empty title -> main page (logo + WEB CONTROL + 4 buttons); otherwise section title and a footer with a back button
// Breadcrumb at the top: "Home · <parent page> · <title>". A sub page (view log, view profile...) calls webCrumb() BEFORE webPage() to add its parent;
// webPage() clears it after use. A level-1 page shows only "Home · <title>".
static bool webPinOn();                        // PIN is on (defined in the PIN section below)
static const char *g_webCrumbKey = nullptr, *g_webCrumbHref = nullptr;
static uint32_t g_webLastHit = 0;               // last time a browser opened a page (Mode button: "Connected" when < 30 s)
static bool webClientRecent() { return g_webOn && g_webLastHit && (millis() - g_webLastHit) < 30000UL; }
static String webPage(const String &title, const String &body, bool home = false, bool pub = false) {   // pub: page before login (shows only the version, no links)
    netTouch();
    g_webLastHit = millis(); if (!g_webLastHit) g_webLastHit = 1;                                    // a user is present: push back the WiFi auto-off time
    const int L = wl();
    if (g_web.hasArg("l") && wlFromCode(g_web.arg("l")) >= 0)
        g_web.sendHeader("Set-Cookie", String("lang=") + wlCode(L) + "; Path=/; Max-Age=31536000; SameSite=Lax");
    String s = String("<!doctype html><html lang=") + wlHtmlLang(L) + "><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'><title>LY AN ROASTER</title><style>";
    s += FPSTR(WEB_CSS_TXT);
    s += F("</style>");
    s += F("<div class=w>");
    s += F("<img class=\"lg\" src=\"/logo.svg\" alt=\"LY AN ROASTER\">");   // logo da co san chu LY AN ROASTER
    if (home) s += F("<div class='m s' style='text-align:center;margin:-12px 0 16px;letter-spacing:.2em'>WEB CONTROL</div>");
    s += webLangBar();
    if (!home) {
        s += "<h2><a href='/'>" + wt("c.home2") + "</a> &middot; ";
        if (g_webCrumbKey && g_webCrumbHref) s += String("<a href='") + g_webCrumbHref + "'>" + wt(g_webCrumbKey) + "</a> &middot; ";
        s += title + "</h2>";
    }
    g_webCrumbKey = g_webCrumbHref = nullptr;
    s += body;
    if (home) {
        String v = FW_VERSION;
        if (v.length() && (v[0] == 'v' || v[0] == 'V')) v.remove(0, 1);
        const String sep = "<span class=m>" + wt("c.sep") + "</span>";
        s += F("<div class=f>");
        if (pub) s += "<span class='m s'>" + wt("ft.ver") + " " + v + "</span>";
        else {
        for (uint8_t i = 0; i < g_webFootN; i++)                                // modules attach footer links via webAddFoot()
            s += String("<a href='") + g_webFoot[i].href + "'>" + wt(g_webFoot[i].key) + "</a>" + sep;
        s += "<a href='/about'>" + wt("ft.about") + "</a>" + sep +
             (webPinOn() ? "<a href='/logout'>" + wt("c.logout") + "</a>" + sep : String()) +
             "<span class='m s' style='margin-left:9px'>" + wt("ft.ver") + " " + v + "</span>";
        }
        s += F("</div>");
    }
    s += F("<script>if(!sessionStorage.getItem('lg_ts')){sessionStorage.setItem('lg_ts','1');"
           "fetch('/api/time',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
           "body:'e='+Math.floor(Date.now()/1000)+'&z='+(-new Date().getTimezoneOffset())}).catch(function(){});}</script>");
    s += F("</div></html>");
    return s;
}

// ---------------- Home page AFTER the PIN: 4 function buttons + a small row Files - Firmware - About - Version
static void webHome() {
    String b;
    b += F("<div class=hn>");                                                   // 4 buttons: shorter, 2 rows x 2 columns
    b += "<a class='b p' href='/wifi'>" + wt("nav.wifi") + "</a>";
    for (uint8_t i = 0; i < g_webNavN; i++)                                     // modules attach nav buttons via webAddNav()
        b += String("<a class=b href='") + g_webNav[i].href + "'>" + wt(g_webNav[i].key) + "</a>";
    b += F("</div>");
    b += F("<div class=c><table>");
    b += "<tr><td class=m>" + wt("idx.wifi") + "</td><td>" + (netOnline() ? netSSID() : wt("idx.notconn")) + "</td></tr>";
    b += "<tr><td class=m>" + wt("idx.addr") + "</td><td>" + (netIP().length() ? netIP() : String("--")) + "</td></tr>";
    b += "<tr><td class=m>" + wt("idx.time") + "</td><td>" + (clockIsValid() ? clockStrFull() : String("--")) + "</td></tr>";
    b += F("</table></div>");
    g_web.send(200, "text/html", webPage("", b, true));
}

// ---------------------------------------------------------------- cai WiFi
static void webWifi() {
    // /scan only STARTS a scan and returns here at once; the scan is not finished, so the list is empty
    // and the page never reloads. While scanning, the page now reloads after 2 s.
    const bool scanning = (netScanState() == NSCAN_RUN);
    String b;
    if (scanning) b += "<meta http-equiv=refresh content=2><div class=c><div class=m>" + wt("w.scanning") + "</div></div>";
    b += "<form method=POST action='/join'><div class=c><div class='m s'>" + wt("w.pick") + "</div><select name=ssid>";
    const int n = (netScanState() == NSCAN_DONE) ? netScanCount() : 0;
    const String openTxt = wt("w.open");
    for (int i = 0; i < n && i < 20; i++)
        b += "<option value='" + netScanSSID(i) + "'>" + netScanSSID(i) +
             "  (" + String(netBars(netScanRSSI(i))) + "/4)" + (netScanLocked(i) ? String() : "  " + openTxt) + "</option>";
    b += F("</select>");
    if (n == 0 && !scanning) b += "<div class='m s'>" + wt("w.noscan") + "</div>";
    b += "<input name=pass type=password placeholder='" + wt("w.pass") + "' autocomplete=off>"
         "<div style='display:flex;gap:8px'>"                                        // Connect + Rescan on one row (Rescan = POST /scan via formaction)
         "<button class='b p' type=submit style='flex:1;min-width:0'>" + wt("w.save") + "</button>"
         "<button class=b type=submit formaction='/scan' formnovalidate style='flex:1;min-width:0'" + String(scanning ? " disabled" : "") + ">" +
         wt(scanning ? "w.rescanning" : "w.rescan") + "</button></div></div></form>";
    if (netSavedCount()) {
        b += "<div class=c><div class='m s'>" + wt("w.saved") + "</div><table>";
        for (uint8_t i = 0; i < netSavedCount(); i++)
            b += "<tr><td>" + String(netSavedSSID(i)) + (netIsDefault(i) ? " <span class=g>" + wt("w.def") + "</span>" : String()) +
                 "</td><td><a href='/forget?i=" + String(i) + "'>" + wt("w.del") + "</a></td></tr>";
        b += F("</table></div>");
    }
    g_web.send(200, "text/html", webPage(wt("nav.wifi"), b));
}

static void webScan()   { netScanStart(); g_web.sendHeader("Location", "/wifi"); g_web.send(303); }
static void webForget() { netForget(g_web.arg("i").toInt()); g_web.sendHeader("Location", "/wifi"); g_web.send(303); }
static void webJoin() {
    const String ssid = g_web.arg("ssid"), pass = g_web.arg("pass");
    if (ssid.length()) netJoin(ssid.c_str(), pass.c_str());
    g_web.send(200, "text/html", webPage(wt("nav.wifi"), "<div class=c>" + wt("w.joined") + "</div>"));
}

// ---------------- about
static String webAboutCard() {
    String b = F("<div class=c><table>");
    struct { const char *k, *v; bool tr; } row[] = {
        { "a.phone", ABOUT_PHONE, true }, { "Zalo", ABOUT_ZALO, false }, { "Website", ABOUT_WEB, false },
        { "Facebook", ABOUT_FACEBOOK, false }, { "Email", ABOUT_EMAIL, false }, { "a.addr", ABOUT_ADDRESS, true },
    };
    if (ABOUT_TAGLINE[0]) b = "<div class=c><div style='text-align:center;color:#BD9940;margin-bottom:10px'>" +
                              String(ABOUT_TAGLINE) + "</div><table>";
    for (auto &r : row) if (r.v && r.v[0]) b += "<tr><td class=m>" + (r.tr ? wt(r.k) : String(r.k)) + "</td><td>" + String(r.v) + "</td></tr>";
    b += "<tr><td class=m>" + wt("a.dev") + "</td><td>" + String(DEVICE_NAME) + "</td></tr>";       // device first, version right below
    b += "<tr><td class=m>" + wt("a.fw") + "</td><td>" + String(FW_VERSION) + "</td></tr>";
    b += F("</table></div>");
    return b;
}
static void webAbout() { g_web.send(200, "text/html", webPage(wt("ft.about"), webAboutCard())); }   // public (no PIN needed)

// Sync time and timezone automatically from the browser (works in AP offline and WiFi STA; handles moving the device to another region)
static void webTimeSync() {
    const time_t epoch = (time_t)strtoll(g_web.arg("e").c_str(), nullptr, 10);
    const int16_t tz   = (int16_t)g_web.arg("z").toInt();
    const bool ok = clockSyncFromBrowser(epoch, tz);
    g_web.sendHeader("Cache-Control", "no-store");
    g_web.send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}


// ---------------- PIN (web access + firmware upload)
// Entry: the first (intro) page asks for the PIN, which unlocks Home and the other pages. Firmware upload (/update) asks for the PIN AGAIN (separate cookie "lgu", 10 min).
// Replaces OTA_USER / OTA_PASS: only the PIN is entered. The PIN comes from WEB_PIN (RoasterSetting.h); the OSS build cannot change it on the screen.
// Random session cookie, expires after 60 min idle, one session at a time. 5 wrong tries -> locked for 60 s. Roast start (web off) -> session ends.
#ifndef WEB_PIN
#define WEB_PIN "1234"
#endif
#define WEB_SESSION_MS 3600000UL
static char     g_pinTok[17] = "";
static uint32_t g_pinUntil = 0;
static uint8_t  g_pinFail = 0;
static uint32_t g_pinLock = 0;                 // != 0: locked until this millis()
static char     g_webPin[33] = WEB_PIN;        // PIN in use (= WEB_PIN)
static const bool g_webPinOn = true;           // web access always needs the PIN (WEB_PIN)

static bool webPinOn() { return g_webPinOn; }

static bool webPinEq(const String &in) {       // constant-time compare
    const char *pin = g_webPin;
    const size_t n = strlen(pin);
    uint8_t d = (in.length() != n);
    for (size_t i = 0; i < n; i++) d |= (uint8_t)(pin[i] ^ (i < in.length() ? in[i] : 0));
    return d == 0;
}
static bool webCookieIs(const char *name, const char *tok, uint32_t until) {   // cookie "name=<tok>" con han
    if (!tok[0] || (int32_t)(until - millis()) <= 0) return false;
    const String c = g_web.header("Cookie");
    const String key = String(name) + "=";
    int i = 0;
    while ((i = c.indexOf(key, i)) >= 0) {
        if (i == 0 || c[i - 1] == ' ' || c[i - 1] == ';') return c.substring(i + key.length(), i + key.length() + 16) == String(tok);
        i += key.length();
    }
    return false;
}
static bool webPinValid() {                    // passed the PIN gate (cookie lgs): each use extends 60 min. PIN off: always valid
    if (!g_webPinOn) return true;
    if (!webCookieIs("lgs", g_pinTok, g_pinUntil)) return false;
    g_pinUntil = millis() + WEB_SESSION_MS;
    return true;
}
static void webPinForm(const String &next, const String &err) {
    String b = webAboutCard();
    b += "<form method=POST action='/login'><div class=c><div class='m s'>" + wt("pin.enter") + "</div>"
         "<input name=pin type=password inputmode=numeric autocomplete=off autofocus required>";
    b += "<input type=hidden name=next value='" + next + "'>";
    if (err.length()) b += "<div class=r style='margin:6px 0'>" + err + "</div>";
    b += "<button class='b p' type=submit>" + wt("pin.ok") + "</button></div></form>";
    g_web.send(200, "text/html", webPage("", b, true, true));
}

static bool webGate() {
    if (webPinValid()) return true;
    String to = "/";
    if (g_web.method() == HTTP_GET && webNextOk(g_web.uri())) to += "?next=" + webEnc(g_web.uri());
    g_web.sendHeader("Location", to);
    g_web.send(303);
    return false;
}

static void webOnG(const char *uri, WebServer::THandlerFunction fn) {
    g_web.on(uri, [fn]() { if (webGate()) fn(); });
}
static void webOnG(const char *uri, HTTPMethod m, WebServer::THandlerFunction fn) {
    g_web.on(uri, m, [fn]() { if (webGate()) fn(); });
}

static void webIndex() {
    if (webPinValid()) { webHome(); return; }
    String next = g_web.arg("next");
    if (!webNextOk(next)) next = "/";
    webPinForm(next, "");
}

static void webLogin() {
    String next = g_web.arg("next");
    if (!webNextOk(next)) next = g_webNextDef;
    if (g_pinLock && (int32_t)(g_pinLock - millis()) > 0) { webPinForm(next, wt("pin.lock")); return; }
    if (webPinEq(g_web.arg("pin"))) {
        g_pinFail = 0; g_pinLock = 0;
        snprintf(g_pinTok, 17, "%08x%08x", (unsigned)esp_random(), (unsigned)esp_random());
        g_pinUntil = millis() + WEB_SESSION_MS;
        g_web.sendHeader("Set-Cookie", String("lgs=") + g_pinTok + "; Path=/; HttpOnly; SameSite=Strict");
        g_web.sendHeader("Location", next);
        g_web.send(303);
        return;
    }
    if (++g_pinFail >= 5) { g_pinFail = 0; g_pinLock = millis() + 60000UL; if (!g_pinLock) g_pinLock = 1; }
    webPinForm(next, wt("pin.bad"));
}

static void webLogout() {
    g_pinTok[0] = 0;
    g_web.sendHeader("Set-Cookie", "lgs=; Path=/; Max-Age=0");
    g_web.sendHeader("Location", "/");
    g_web.send(303);
}

// ---------------- init / run loop
static void webStart() {
    if (g_webOn) return;
    g_web.on("/", webIndex);
    webOnG("/wifi", webWifi);
    g_web.on("/about", webAbout);
    g_web.on("/api/time", HTTP_POST, webTimeSync);
    g_web.on("/logo.svg", []() {                           // vector logo in flash (browser caches it for 1 day)
        g_web.sendHeader("Cache-Control", "max-age=86400");
        g_web.sendHeader("Content-Encoding", "gzip");       // already-compressed SVG
        g_web.send_P(200, "image/svg+xml", (PGM_P)WEB_LOGO_SVGZ, sizeof(WEB_LOGO_SVGZ));
    });
    webOnG("/scan", HTTP_POST, webScan);
    webOnG("/forget", webForget);
    webOnG("/join", HTTP_POST, webJoin);
    g_web.on("/login", HTTP_POST, webLogin);
    g_web.on("/logout", webLogout);
    for (uint8_t i = 0; i < g_webRoutesN; i++) g_webRoutes[i]();   // extra routes registered by modules (webAddRoutes)
    g_web.onNotFound([]() {                                  // own access point: every address goes to the main page
        if (netApOn()) { g_web.sendHeader("Location", String("http://") + WiFi.softAPIP().toString()); g_web.send(302); }
        else g_web.send(404, "text/plain", "404");
    });
    static const char *hdrs[] = { "Cookie" };
    g_web.collectHeaders(hdrs, 1);
    g_web.begin();
    g_webOn = true;
}

// Close the web server + DNS + OTA (called when WiFi goes fully off): returns sockets and RAM.
static void webStop() {
    if (!g_webOn) return;
    if (g_dnsOn) { g_dns.stop(); g_dnsOn = false; }
    g_web.close();
    g_webOn = false;
    g_pinTok[0] = 0;                                        // end PIN session
    g_webLastHit = 0;
}

// Builds with a screen: the web runs ONLY in Web portal mode (webWanted, Mode button). Screenless builds: WiFi up = web up.
static inline bool webAllowed() {
#if ENABLE_UI
    return webWanted;
#else
    return true;
#endif
}
static void webTick() {
    if (!netRadioOn()) { webStop(); return; }               // WiFi is off: nothing to serve
    if (webRoastBusy()) { webStop(); return; }              // ROASTING (START pressed): web control is shut down to free RAM
    if (!webAllowed()) { webStop(); return; }               // not in Web portal mode (e.g. Artisan uses WiFi) -> do not open the web
    if (!g_webOn) {
        if (netOnline() || netApOn()) webStart();
        return;
    }
    if (netApOn() && !g_dnsOn) { g_dns.start(53, "*", WiFi.softAPIP()); g_dnsOn = true; }
    else if (!netApOn() && g_dnsOn) { g_dns.stop(); g_dnsOn = false; }
    if (g_dnsOn) g_dns.processNextRequest();
    g_web.handleClient();
}


// Call once in setup(): registers webStop() so RoasterNet closes the web server BEFORE WiFi goes off.
static void webInit() {
    static bool done = false;
    if (done) return;
    done = true;
    radioOnWifiStop(webStop);
}

#else
static inline void webInit() {}
static inline void webTick() {}
#endif  // ENABLE_WIFI && ENABLE_WEB_PORTAL
