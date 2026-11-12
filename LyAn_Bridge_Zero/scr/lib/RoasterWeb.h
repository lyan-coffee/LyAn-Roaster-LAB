/*
 * GPL v3
 * Trang web (chế độ 3): cài WiFi, cấu hình Artisan, nạp firmware (OTA), đổi PIN.
 * Địa chỉ: http://192.168.4.1 (điểm phát riêng) hoặc http://lyanzero.local
 * Đang cấp nhiệt thì web tắt để nhường RAM cho điều khiển.
 */
#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "../RoasterSetting.h"
#include "RoasterState.h"
#include "RoasterControl.h"
#include "RoasterRadio.h"
#include "RoasterRGB.h"
#include "RoasterNet.h"
#include "RoasterAsetData.h"
#include "RoasterWebLogo.h"

static WebServer  g_web(WEB_SERVER_PORT);
static DNSServer  g_dns;                        // điểm phát riêng: mọi địa chỉ về 192.168.4.1
static bool       g_webOn = false, g_dnsOn = false;
static bool       g_otaBusy = false;
static int        g_otaPct = 0;                 // 0..100, -1 = lỗi
static int        g_otaLen = 0;                 // độ dài file tải lên (để tính %)
static String     g_otaMsg;

#include "RoasterCloud.h"            // cập nhật từ Cloud

// Đang cấp nhiệt thì không cho nạp firmware. Zero không có màn hình nên webBusy() trống.
static bool webRoastBusy() { return controlBusy(); }
static inline void webBusy(int, bool) {}

#include "RoasterWebI18n.h"          // chữ 3 ngôn ngữ

// Khung trang chung
static const char WEB_CSS_TXT[] PROGMEM =
  ":root{--g:#BD9940;--cr:#F3E5D0;--m:#9C8C7C}\n"
  "*{box-sizing:border-box}\n"
  "body{margin:0;min-height:100vh;padding:22px 16px 28px;"
  "background:linear-gradient(180deg,#0F1A28,#070B11);background-attachment:fixed;color:var(--cr);"
  "font:16px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}\n"
  ".w{max-width:430px;margin:0 auto}\n"
  ".lg{display:block;margin:4px auto 20px;width:188px;height:188px}\n"
  "h2{font-size:17px;color:var(--g);margin:0 0 14px;font-weight:600}\n"
  "h2 a{color:inherit}\n"
  "a{color:var(--g);text-decoration:none}\n"
  ".b{display:block;width:100%;padding:17px;margin:11px 0;border:1px solid #2d3a4a;border-radius:14px;"
  "background:rgba(189,153,64,.07);color:var(--cr);font-size:17px;font-weight:600;text-align:center;cursor:pointer}\n"
  ".b:active{background:rgba(189,153,64,.2)}.b.p{background:var(--g);color:#0B1018;border-color:var(--g)}\n"
  ".c{background:rgba(255,255,255,.04);border:1px solid #243041;border-radius:14px;padding:15px;margin:12px 0}\n"
  "input,select{width:100%;background:#0B121C;color:var(--cr);border:1px solid #2d3a4a;border-radius:10px;"
  "padding:12px;margin:7px 0;font-size:16px}\n"
  // ô chọn: thay mũi tên mặc định bằng mũi tên riêng, cách lề phải 16px
  "select{-webkit-appearance:none;-moz-appearance:none;appearance:none;padding-right:44px;cursor:pointer;text-overflow:ellipsis;"
  "background-image:url(\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='14' height='8' viewBox='0 0 14 8'%3E%3Cpath d='M1 1l6 6 6-6' "
  "fill='none' stroke='%239C8C7C' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'/%3E%3C/svg%3E\");"
  "background-repeat:no-repeat;background-position:right 16px center;background-size:14px 8px}\n"
  "select:focus{outline:none;border-color:var(--g)}select option{background:#0B121C;color:var(--cr)}\n"
  "table{width:100%;border-collapse:collapse}td{padding:7px 0;border-bottom:1px solid #1b2430;vertical-align:top}\n"
  "td+td{text-align:right}.m{color:var(--m)}.s{font-size:15px}.g{color:#43D17A}.r{color:#ff6b6b}\n"
  ".f{text-align:center;margin-top:26px}.f a{margin:0 9px;font-size:15px;color:var(--m)}\n"
  ".lb{text-align:center;margin:-10px 0 16px;font-size:15px}.lb a{margin:0 9px;color:var(--m)}.lb b{margin:0 9px;color:var(--g)}\n";

#define WEB_NEXT_MAX   8
static const char *g_webNext[WEB_NEXT_MAX]; static uint8_t g_webNextN = 0;
#if ENABLE_OTA_WEB
static const char *g_webNextDef = "/update";     // trang mở sau khi nhập PIN
#else
static const char *g_webNextDef = "/";
#endif

static void webAddNext(const char *path, bool makeDefault = false) {
    if (g_webNextN < WEB_NEXT_MAX) g_webNext[g_webNextN++] = path;
    if (makeDefault) g_webNextDef = path;
}
static bool webNextOk(const String &n) {
    if (n == "/pin") return true;
#if ENABLE_OTA_WEB
    if (n == "/update") return true;
#endif
    for (uint8_t i = 0; i < g_webNextN; i++) if (n == g_webNext[i]) return true;
    return false;
}

// Khung "Đang cập nhật" + thanh tiến trình
// Gửi form bằng XMLHttpRequest để có % thật. Nạp xong máy tự khởi động lại.
static String webUpHtml() {
    String s = F("<div id=ub style='display:none'><div class=c style='border-color:#ff6b6b'><b class=r>");
    s += wt("up.warn");
    s += F("</b><div style='height:12px;background:#0B121C;border:1px solid #2d3a4a;border-radius:8px;margin:12px 0 6px;overflow:hidden'>"
           "<div id=ubar style='height:100%;width:0;background:#BD9940'></div></div>"
           "<div id=ut class='m s'>0%</div></div></div>");
    s += "<script>var lgBusy=0,LG={rs:'" + wtJs("up.restart") + "',wr:'" + wtJs("up.writing") + "',lf:'" + wtJs("up.left") +
         "',ca:'" + wtJs("up.calc") + "',ne:'" + wtJs("up.neterr") + "'};";
    s += F("window.onbeforeunload=function(e){if(lgBusy){e.preventDefault();e.returnValue='';return ''}};\n"
           "function lgSay(t){document.getElementById('ut').textContent=t}\n"
           "function lgBoot(){\n"
           " var n=20,b=document.getElementById('ubar');b.style.width='100%';lgBusy=1;\n"
           " var iv=setInterval(function(){\n"
           "  lgSay(LG.rs.split('{0}').join(n));\n"
           "  if(--n<0){clearInterval(iv);var p=function(){fetch('/',{cache:'no-store'}).then(function(r){if(r.ok){lgBusy=0;location.href='/'}else setTimeout(p,2000)}).catch(function(){setTimeout(p,2000)})};p()}\n"
           " },1000)}\n"
           "function lgUp(f,m){\n"
           " var x=new XMLHttpRequest(),t0=Date.now(),full=0,bar=document.getElementById('ubar');\n"
           " document.getElementById('ub').style.display='block';f.style.display='none';lgBusy=1;\n"
           " x.open('POST',f.action);\n"
           " x.upload.onprogress=function(e){if(!e.lengthComputable)return;\n"
           "  var p=Math.floor(e.loaded*100/e.total),s=(Date.now()-t0)/1000,r=(s>1&&e.loaded>0)?Math.ceil(s*(e.total-e.loaded)/e.loaded):0;\n"
           "  bar.style.width=p+'%';if(e.loaded>=e.total)full=1;\n"
           "  lgSay(p+'%  -  '+(p>=100?LG.wr:(r?LG.lf.split('{0}').join(r):LG.ca)))};\n"
           " x.onload=function(){\n"
           "  if(m==1&&x.responseText.indexOf('<!--LGOK-->')>=0){lgBoot();return}\n"
           "  lgBusy=0;\n"
           "  if(m==2&&x.responseText.indexOf('class=r')<0){location.href=x.responseURL||'/files';return}\n"
           "  document.open();document.write(x.responseText);document.close()};\n"
           " x.onerror=function(){if(m==1&&full){lgBoot();return}\n"
           "  lgBusy=0;lgSay(LG.ne)};\n"
           " x.send(new FormData(f));return false}\n"
           "</script>");
    return s;
}

static String webEnc(const String &s) {                       // mã hoá URL cho phần truy vấn
    String o;
    o.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '/') o += (char)c;
        else { char t[4]; snprintf(t, sizeof t, "%%%02X", c); o += t; }
    }
    return o;
}

// Thanh "EN | VI | 中文" dưới tiêu đề
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

// home = trang chính; trang khác có "Home · tiêu đề" ở đầu
static String webPage(const String &title, const String &body, bool home = false) {
    netTouch();                                    // có người dùng: chưa tự tắt WiFi
    const int L = wl();
    if (g_web.hasArg("l") && wlFromCode(g_web.arg("l")) >= 0)
        g_web.sendHeader("Set-Cookie", String("lang=") + wlCode(L) + "; Path=/; Max-Age=31536000; SameSite=Lax");
    String s = String("<!doctype html><html lang=") + wlHtmlLang(L) + "><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'><title>LY AN ROASTER</title><style>";
    s += FPSTR(WEB_CSS_TXT);
    s += F("</style><div class=w><img class=\"lg\" src=\"/logo.svg\" alt=\"LY AN ROASTER\">");
    if (home) s += F("<div class='m s' style='text-align:center;margin:-12px 0 16px;letter-spacing:.2em'>WEB CONTROL</div>");
    s += webLangBar();
    if (!home) s += "<h2><a href='/'>" + wt("c.home2") + "</a> &middot; " + title + "</h2>";   // Home · <tiêu đề>
    s += body;
    if (home) {
        String v = FW_VERSION;
        if (v.length() && (v[0] == 'v' || v[0] == 'V')) v.remove(0, 1);
        const String sep = "<span class=m>" + wt("c.sep") + "</span>";
        s += "<div class=f><a href='/update'>" + wt("ft.fw") + "</a>" + sep +
             "<a href='/pin'>" + wt("ft.pin") + "</a>" + sep +
             "<a href='/about'>" + wt("ft.about") + "</a>" + sep +
             "<span class='m s' style='margin-left:9px'>" + wt("ft.ver") + " " + v + "</span></div>";
    }
    s += F("</div></html>");
    return s;
}

// Cập nhật từ Cloud: thẻ thông báo
#if CLOUD_ON
static String clFmt(const char *key, const String &a) { String s = wt(key); s.replace("{0}", a); return s; }
static String clEsc(const char *s) {                          // chữ an toàn để chèn vào HTML
    String o;
    for (; *s; s++) {
        if (*s == '&') o += "&amp;";
        else if (*s == '<') o += "&lt;";
        else if (*s == '>') o += "&gt;";
        else if (*s == '"') o += "&quot;";
        else if (*s == '\'') o += "&#39;";
        else o += *s;
    }
    return o;
}
static bool clKnowsNew() { return g_clVer[0] && (g_clState == CL_NEW || g_clState == CL_FAIL); }

// Trang chính: chỉ hiện khi có bản mới
static String webCloudNotice() {
    if (!clKnowsNew()) return String();
    return "<a class=c style='display:block;text-align:center;border-color:#43D17A' href='/update'><b class=g>" +
           clFmt("cl.new", clEsc(g_clVer)) + "</b><div class='m s'>" + wt("cl.tap") + "</div></a>";
}

// Thẻ Cloud ở đầu trang /update
static String webCloudCard() {
    const uint8_t st = g_clState;
    String b = "<div class=c><b>" + wt("cl.title") + "</b>";
    if (clKnowsNew()) {
        b += "<div class=g style='margin:8px 0'>" + clFmt("cl.new", clEsc(g_clVer)) + "</div><table>"
             "<tr><td class=m>" + wt("cl.size") + "</td><td>" + String(g_clSize / 1048576.0, 2) + " MB</td></tr>";
        if (g_clNotes[0]) b += "<tr><td class=m>" + wt("cl.notes") + "</td><td>" + clEsc(g_clNotes) + "</td></tr>";
        b += "</table>";
        if (st == CL_FAIL && g_clErr) b += "<div class=r style='margin:8px 0'>" + wt("u.err") + wt(g_clErr) + "</div>";
        b += "<form method=POST action='/cloudupdate'><button class='b p' type=submit>" + wt("cl.btn") + "</button></form>";
    } else if (st == CL_UP_TO_DATE) {
        b += "<div class=g style='margin:8px 0'>" + wt("cl.uptodate") + "</div>";
    } else if (st == CL_CHECKING) {
        b += "<meta http-equiv=refresh content=4;url=/update><div class=m style='margin:8px 0'>" + wt("cl.checking") + "</div>";
    } else if (st == CL_ERR_FMT) {
        b += "<div class=r style='margin:8px 0'>" + wt("cl.fmt") + "</div>";
    } else if (st == CL_ERR_NET) {
        b += "<div class=r style='margin:8px 0'>" + wt(g_clErr ? g_clErr : "cl.neterr") + "</div>";
    } else {
        b += "<div class='m s' style='margin:8px 0'>" + wt("cl.idle") + "</div>";
    }
    if (st != CL_CHECKING)
        b += "<form method=POST action='/cloudcheck'><button class=b type=submit>" + wt("cl.recheck") + "</button></form>";
    b += "</div>";
    return b;
}
#endif

// Trang chính
static void webIndex() {
    String b;
#if CLOUD_ON
    b = webCloudNotice();
#endif
    b += "<a class='b p' href='/wifi'>" + wt("nav.wifi") + "</a>"
         "<a class=b href='/artisan'>" + wt("nav.artisan") + "</a>";
    b += F("<div class=c><table>");
    b += "<tr><td class=m>" + wt("idx.wifi") + "</td><td>" + (netOnline() ? netSSID() : wt("idx.notconn")) + "</td></tr>";
    b += "<tr><td class=m>" + wt("idx.addr") + "</td><td>" + (netIP().length() ? netIP() : String("--")) + "</td></tr>";
    b += F("</table></div>");
    g_web.send(200, "text/html", webPage("", b, true));
}

// Cài WiFi
static void webWifi() {
    // /scan chỉ bắt đầu quét; trang tự tải lại sau 2 giây
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
         "<button class='b p' type=submit>" + wt("w.save") + "</button></div></form>"
         "<form method=POST action='/scan'><button class=b type=submit" + String(scanning ? " disabled" : "") + ">" +
         wt(scanning ? "w.rescanning" : "w.rescan") + "</button></form>";
    if (netSavedCount()) {
        b += "<div class=c><div class='m s'>" + wt("w.saved") + "</div><table>";
        for (uint8_t i = 0; i < netSavedCount(); i++)
            b += "<tr><td>" + String(netSavedSSID(i)) + "</td><td><a href='/forget?i=" + String(i) + "'>" + wt("w.del") + "</a></td></tr>";
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

// Artisan: địa chỉ + file cài đặt .aset
// Chỉ thay host= và port= trong file .aset nhúng sẵn
// Gửi từng mảnh từ flash
static void webAsetPart(PGM_P p, size_t n) {
    while (n) {
        const size_t k = n > 1024 ? 1024 : n;
        g_web.sendContent_P(p, k);
        p += k;
        n -= k;
    }
}

static String webArtisanIp() { return netOnline() ? WiFi.localIP().toString() : WiFi.softAPIP().toString(); }

static void webAset() {
    const String ip   = webArtisanIp();
    const String port = String(ARTISAN_WS_PORT);
    const size_t n0 = sizeof(ASET_P0) - 1, n1 = sizeof(ASET_P1) - 1, n2 = sizeof(ASET_P2) - 1;
    g_web.sendHeader("Content-Disposition", "attachment; filename=\"LyAn Roaster.aset\"");
    g_web.setContentLength(n0 + ip.length() + n1 + port.length() + n2);
    g_web.send(200, "application/octet-stream", "");
    webAsetPart(ASET_P0, n0);
    g_web.sendContent(ip);
    webAsetPart(ASET_P1, n1);
    g_web.sendContent(port);
    webAsetPart(ASET_P2, n2);
}

static void webArtisan() {
    String b = "<div class=c><div class=m>" + wt("ar.addr") + "</div><div>ws://" + webArtisanIp() + ":" + String(ARTISAN_WS_PORT) + "/</div></div>"
               "<div class=c><a class='b p' href='/artisan.aset'>" + wt("ar.dl") + "</a>"
               "<div class='m s'>" + wt("ar.dlnote") + "</div></div>"
               "<div class=c><div class=m>" + wt("ar.after") + "</div><div class='m s'>" + wt("ar.steps") + "</div></div>"
               "<div class=c><div class=m>" + wt("ar.btns") + "</div><div class='m s'>" + wt("ar.btnstxt") + "</div></div>";
    g_web.send(200, "text/html", webPage(wt("nav.artisan"), b));
}

// Mã PIN (cho nạp firmware)
// Đăng nhập: phiên 15 phút. Sai 5 lần -> khoá 60 giây.
#ifndef WEB_PIN
#define WEB_PIN "0918393391"           // PIN mặc định
#endif
#ifndef FACTORY_BOOT_MS
#define FACTORY_BOOT_MS 10000
#endif
#define WEB_SESSION_MS 900000UL
static char     g_pinTok[17] = "";
static uint32_t g_pinUntil = 0;
static uint8_t  g_pinFail = 0;
static uint32_t g_pinLock = 0;                 // != 0: đang khoá đến mốc này
static char     g_webPin[33] = WEB_PIN;        // PIN đang dùng (lưu NVS)
static uint32_t g_facPromptUntil = 0;          // != 0: vừa nhập PIN mặc định khi PIN đã đổi -> hỏi Reset Factory

static bool webPinFmtOk(const char *s) {       // 4..10 chữ số
    if (!s) return false;
    const size_t n = strlen(s);
    if (n < 4 || n > 10) return false;
    for (size_t i = 0; i < n; i++) if (s[i] < '0' || s[i] > '9') return false;
    return true;
}
static void webPinLoad() {                     // gọi trong setup()
    Preferences p;
    if (!p.begin("lyweb", true)) return;
    if (p.isKey("wpin")) {
        char b[33] = {0};
        p.getString("wpin", b, sizeof b);
        if (webPinFmtOk(b)) snprintf(g_webPin, sizeof g_webPin, "%s", b);
    }
    p.end();
}
static bool webPinSet(const char *pin) {
    if (!webPinFmtOk(pin)) return false;
    snprintf(g_webPin, sizeof g_webPin, "%s", pin);
    Preferences p;
    if (p.begin("lyweb", false)) { p.putString("wpin", g_webPin); p.end(); }
    g_pinTok[0] = 0; g_pinFail = 0; g_pinLock = 0;   // hết phiên: đăng nhập lại
    return true;
}
static bool webPinIsDefault() { return strcmp(g_webPin, WEB_PIN) == 0; }
static bool webCmpPin(const char *pin, const String &in) {   // so sánh hết chuỗi
    const size_t n = strlen(pin);
    uint8_t d = (in.length() != n);
    for (size_t i = 0; i < n; i++) d |= (uint8_t)(pin[i] ^ (i < in.length() ? in[i] : 0));
    return d == 0;
}

static bool webPinEq(const String &in) { return webCmpPin(g_webPin, in); }
static bool webPinValid() {
    if (!g_pinTok[0] || (int32_t)(g_pinUntil - millis()) <= 0) return false;
    const String c = g_web.header("Cookie");
    const int i = c.indexOf("lgs=");
    if (i < 0 || c.substring(i + 4, i + 20) != String(g_pinTok)) return false;
    g_pinUntil = millis() + WEB_SESSION_MS;
    return true;
}
static void webPinForm(const String &next, const String &err) {
    String b = "<form method=POST action='/login'><div class=c><div class='m s'>" + wt("pin.enter") + "</div>"
               "<input name=pin type=password inputmode=numeric autocomplete=off autofocus required>";
    b += "<input type=hidden name=next value='" + next + "'>";
    if (err.length()) b += "<div class=r style='margin:6px 0'>" + err + "</div>";
    b += "<button class='b p' type=submit>" + wt("pin.ok") + "</button></div></form>";
    g_web.send(200, "text/html", webPage(wt("pin.title"), b));
}
// Trang cần PIN: chưa đăng nhập thì hiện form nhập PIN
static bool webPinGate(const String &next) {
    if (webPinValid()) return true;
    webPinForm(next, "");
    return false;
}
static void webLogin() {
    String next = g_web.arg("next");
    if (!webNextOk(next)) next = g_webNextDef;
    if (g_pinLock && (int32_t)(g_pinLock - millis()) > 0) { webPinForm(next, wt("pin.lock")); return; }
    if (webPinEq(g_web.arg("pin"))) {
        g_pinFail = 0; g_pinLock = 0;
        snprintf(g_pinTok, sizeof g_pinTok, "%08x%08x", (unsigned)esp_random(), (unsigned)esp_random());
        g_pinUntil = millis() + WEB_SESSION_MS;
        g_web.sendHeader("Set-Cookie", String("lgs=") + g_pinTok + "; Path=/; HttpOnly; SameSite=Strict");
        g_web.sendHeader("Location", next);
        g_web.send(303);
        return;
    }
    if (!webPinIsDefault() && webCmpPin(WEB_PIN, g_web.arg("pin"))) {   // PIN đã đổi mà nhập PIN mặc định -> hỏi Reset Factory
        g_facPromptUntil = millis() + 60000UL; if (!g_facPromptUntil) g_facPromptUntil = 1;
        String b = "<div class=c>" + wt("fac.ask") + "</div>"
                   "<form method=POST action='/factory'><input type=hidden name=a value=yes><button class='b p' type=submit>" + wt("fac.yes") + "</button></form>"
                   "<form method=POST action='/factory'><input type=hidden name=a value=no><button class=b type=submit>" + wt("fac.no") + "</button></form>";
        g_web.send(200, "text/html", webPage(wt("fac.title"), b));
        return;
    }
    if (++g_pinFail >= 5) { g_pinFail = 0; g_pinLock = millis() + 60000UL; if (!g_pinLock) g_pinLock = 1; }
    webPinForm(next, wt("pin.bad"));
}
// Reset Factory: chọn Yes rồi nhấn BOOT trong FACTORY_BOOT_MS
static void webFactory() {
    if (g_web.arg("a") == "yes" && g_facPromptUntil && (int32_t)(g_facPromptUntil - millis()) > 0) {
        g_facPromptUntil = 0;
        g_factoryUntil = millis() + FACTORY_BOOT_MS; if (!g_factoryUntil) g_factoryUntil = 1;
        String b = "<meta http-equiv=refresh content=12;url=/><div class=c>" + wt("fac.boot") + "</div>";
        g_web.send(200, "text/html", webPage(wt("fac.title"), b));
        return;
    }
    g_facPromptUntil = 0;
    g_web.sendHeader("Location", "/");
    g_web.send(303);
}
static void webFactoryDo() {                   // gọi từ loop() khi nhấn BOOT xác nhận
    g_factoryUntil = 0;
    { Preferences p; if (p.begin("lynet", false)) { p.clear(); p.end(); } }   // xoá WiFi đã lưu
    { Preferences p; if (p.begin("lyweb", false)) { p.clear(); p.end(); } }   // xoá PIN
    rgbWrite(255, 255, 255, true);
    delay(500);
    ESP.restart();
}
// Đổi mã PIN: nhập PIN hiện tại + PIN mới 2 lần
static void webPinChangeForm(const String &err) {
    String b = "<form method=POST action='/pinset'><div class=c>"
               "<div class='m s'>" + wt("pin.cur") + "</div><input name=cur type=password inputmode=numeric autocomplete=off required>"
               "<div class='m s'>" + wt("pin.new") + "</div><input name=n1 type=password inputmode=numeric autocomplete=off maxlength=10 required>"
               "<div class='m s'>" + wt("pin.new2") + "</div><input name=n2 type=password inputmode=numeric autocomplete=off maxlength=10 required>";
    if (err.length()) b += "<div class=r style='margin:6px 0'>" + err + "</div>";
    b += "<button class='b p' type=submit>" + wt("pin.save") + "</button></div></form>";
    g_web.send(200, "text/html", webPage(wt("pin.change"), b));
}
static void webPinPage() { if (webPinGate("/pin")) webPinChangeForm(""); }
static void webPinSetDo() {
    if (!webPinValid()) { g_web.sendHeader("Location", "/pin"); g_web.send(303); return; }
    if (!webPinEq(g_web.arg("cur"))) { webPinChangeForm(wt("pin.bad")); return; }
    const String n1 = g_web.arg("n1");
    if (!webPinFmtOk(n1.c_str())) { webPinChangeForm(wt("pin.e_fmt")); return; }
    if (n1 != g_web.arg("n2")) { webPinChangeForm(wt("pin.e_match")); return; }
    webPinSet(n1.c_str());
    g_web.sendHeader("Set-Cookie", "lgs=; Path=/; Max-Age=0");
    g_web.send(200, "text/html", webPage(wt("pin.change"), "<div class=c>" + wt("pin.done") + "</div>"));
}
static void webLogout() {
    g_pinTok[0] = 0;
    g_web.sendHeader("Set-Cookie", "lgs=; Path=/; Max-Age=0");
    g_web.sendHeader("Location", "/");
    g_web.send(303);
}

// Nạp firmware (OTA)
// Dung lượng ô trống thật, vd "1.9 MB"
static String webSlotSize() {
    const esp_partition_t *p = esp_ota_get_next_update_partition(nullptr);
    return p ? String((double)p->size / 1048576.0, 1) + " MB" : String("?");
}

#if CLOUD_ON
// Trang tiến trình cài từ Cloud (hỏi /cloudstat mỗi 0,7 giây)
static void webCloudStat() {                                  // không cần PIN
    g_web.sendHeader("Cache-Control", "no-store");
    g_web.send(200, "application/json", String("{\"s\":") + (int)g_clState + ",\"p\":" + (int)g_clPct + "}");
}
static void webCloudCheck() {                                 // nút "Kiểm tra lại"
    if (!webPinValid()) { g_web.sendHeader("Location", "/update"); g_web.send(303); return; }
    cloudRecheckNow();
    g_web.sendHeader("Location", "/update");
    g_web.send(303);
}
static void webCloudInstall() {                               // nút "Tải về và cài đặt"
    if (!webPinValid() || webRoastBusy() || !clKnowsNew()) { g_web.sendHeader("Location", "/update"); g_web.send(303); return; }
    if (!clStartTask(1)) {                                    // thiếu RAM: báo lỗi
        if (!g_clTaskBusy) g_clState = CL_FAIL;
        g_web.sendHeader("Location", "/update");
        g_web.send(303);
        return;
    }
    String b = webUpHtml();                                   // khung "Đang cập nhật" + thanh tiến trình
    b += "<script>document.getElementById('ub').style.display='block';lgBusy=1;var sn=0;\n"
         "function pl(){fetch('/cloudstat',{cache:'no-store'}).then(function(r){return r.json()}).then(function(j){\n"
         " if(j.s==6){sn=1;document.getElementById('ubar').style.width=j.p+'%';lgSay(j.p+'%')}\n"
         " else if(j.s==7){lgBoot();return}\n"
         " else if(j.s==8){lgBusy=0;location.href='/update';return}\n"
         " setTimeout(pl,700)}).catch(function(){if(sn){lgBoot()}else setTimeout(pl,1500)})}\n"
         "pl();</script>";
    g_web.send(200, "text/html", webPage(wt("cl.title"), b));
}
#endif

#if ENABLE_OTA_WEB
static void webUpdateForm() {
    if (!webPinGate("/update")) return;
    String b;
    if (webRoastBusy()) {
        b = "<div class=c>" + wt("u.busy") + "</div>";
    } else {
#if CLOUD_ON
        b = webCloudCard();                                   // thẻ Cloud phía trên
#endif
        b += "<form method=POST action='/doupdate' enctype='multipart/form-data' onsubmit='return lgUp(this,1)'><div class=c>"
            "<div class='m s'>" + wt("u.choose") + "</div>"
            "<input type=file name=f accept='.bin' required>"
            "<button class='b p' type=submit>" + wt("u.btn") + "</button></div></form>"
            "<div class='c m s'>" + wt("u.note") + "</div>";
    }
    b += webUpHtml();
    b += "<div class=c><table><tr><td class=m>" + wt("u.run") + "</td><td>" + String(FW_VERSION) + "</td></tr>"
         "<tr><td class=m>" + wt("u.max") + "</td><td>" + webSlotSize() + "</td></tr></table></div>";
    g_web.send(200, "text/html", webPage(wt("u.title"), b));
}

static void webUpdateDone() {
    const bool ok = (g_otaPct == 100) && !Update.hasError();      // chỉ coi là xong khi đã ghi đủ 100%
    String b;
    if (ok) b = "<!--LGOK--><div class=c>" + wt("u.ok") + "</div>";           // đánh dấu "nạp xong" cho trình duyệt
    else    b = "<div class=c><span class=r>" + wt("u.err") + "</span>" + wt(g_otaMsg.length() ? g_otaMsg.c_str() : "u.e_bad") + "</div>";
    g_web.sendHeader("Connection", "close");
    g_web.send(200, "text/html", webPage(wt("u.title"), b));
    if (ok) { delay(400); ESP.restart(); }
}

// g_otaMsg giữ mã lỗi (u.e_*)
static void webUpdateUpload() {
    netHold(true);                                 // đang nạp: không tự tắt WiFi
    HTTPUpload &up = g_web.upload();
    if (up.status == UPLOAD_FILE_START) {
        g_otaMsg = ""; g_otaPct = 0;
        g_otaLen = g_web.clientContentLength();     // độ dài file tải lên
        if (webRoastBusy()) { g_otaMsg = "u.e_busy"; g_otaPct = -1; netHold(false); return; }
        g_otaBusy = true;
        g_updating = true;                          // đèn hồng chớp
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { g_otaMsg = "u.e_open"; g_otaPct = -1; g_updating = false; }
        else webBusy(0, true);
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (g_otaPct < 0 || !g_otaBusy) return;
        if (Update.write(up.buf, up.currentSize) != up.currentSize) { g_otaMsg = "u.e_write"; g_otaPct = -1; }
        else if (g_otaLen > 0) {
            g_otaPct = (int)((uint64_t)up.totalSize * 100 / (uint64_t)g_otaLen);
            static int shown = -10;
            if (g_otaPct >= shown + 3 || g_otaPct < shown) { shown = g_otaPct; webBusy(g_otaPct, true); }
        }
    } else if (up.status == UPLOAD_FILE_END) {
        if (g_otaPct >= 0 && Update.end(true)) { g_otaPct = 100; webBusy(100, true); }
        else { if (g_otaMsg.length() == 0) { g_otaMsg = "u.e_bad"; g_otaPct = -1; } webBusy(0, false); g_updating = false; }   // xong: đèn hồng giữ đến lúc khởi động lại
        g_otaBusy = false;
        netHold(false);                            // cho phép tự tắt WiFi lại
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort(); g_otaBusy = false; g_otaPct = -1; g_otaMsg = "u.e_abort"; webBusy(0, false); g_updating = false;
        netHold(false);
    }
}
#endif  // ENABLE_OTA_WEB

// Giới thiệu
static void webAbout() {
    String b = F("<div class=c><table>");
    struct { const char *k, *v; } row[] = {
        { "Phone", ABOUT_PHONE }, { "Zalo", ABOUT_ZALO }, { "Website", ABOUT_WEB },
        { "Facebook", ABOUT_FACEBOOK }, { "Email", ABOUT_EMAIL }, { nullptr, ABOUT_ADDRESS },
    };
    if (ABOUT_TAGLINE[0]) b = "<div class=c><div style='text-align:center;color:#BD9940;margin-bottom:10px'>" +
                              String(ABOUT_TAGLINE) + "</div><table>";
    for (auto &r : row) if (r.v && r.v[0]) b += "<tr><td class=m>" + (r.k ? String(r.k) : wt("a.addr")) + "</td><td>" + String(r.v) + "</td></tr>";
    b += "<tr><td class=m>" + wt("a.fw") + "</td><td>" + String(FW_VERSION) + "</td></tr>";
    b += "<tr><td class=m>" + wt("a.dev") + "</td><td>" + String(DEVICE_NAME) + "</td></tr>";
    b += F("</table></div>");
    g_web.send(200, "text/html", webPage(wt("ft.about"), b));
}

// Khởi tạo / vòng chạy
static void webStart() {
    if (g_webOn) return;
    static bool s_routes = false;                            // chỉ đăng ký một lần
    if (!s_routes) {
    s_routes = true;
    g_web.on("/", webIndex);
    g_web.on("/wifi", webWifi);
    g_web.on("/about", webAbout);
    g_web.on("/logo.svg", []() {                             // logo (trình duyệt nhớ 1 ngày)
        g_web.sendHeader("Cache-Control", "max-age=86400");
        g_web.sendHeader("Content-Encoding", "gzip");
        g_web.send_P(200, "image/svg+xml", (PGM_P)WEB_LOGO_SVGZ, sizeof(WEB_LOGO_SVGZ));
    });
    g_web.on("/scan", HTTP_POST, webScan);
    g_web.on("/forget", webForget);
    g_web.on("/join", HTTP_POST, webJoin);
    g_web.on("/artisan", webArtisan);
    g_web.on("/artisan.aset", webAset);
    g_web.on("/update", webUpdateForm);
    g_web.on("/login", HTTP_POST, webLogin);
    g_web.on("/logout", webLogout);
    g_web.on("/pin", webPinPage);
    g_web.on("/pinset", HTTP_POST, webPinSetDo);
    g_web.on("/factory", HTTP_POST, webFactory);
    g_web.on("/doupdate", HTTP_POST,
             []() { if (webPinValid()) webUpdateDone(); else { g_web.sendHeader("Location", "/update"); g_web.send(303); } },
             []() { if (webPinValid()) webUpdateUpload(); });
#if CLOUD_ON
    g_web.on("/cloudupdate", HTTP_POST, webCloudInstall);    // cài từ Cloud (cần PIN)
    g_web.on("/cloudcheck", HTTP_POST, webCloudCheck);       // kiểm tra lại ngay (cần PIN)
    g_web.on("/cloudstat", webCloudStat);                    // tiến trình (chỉ 2 con số)
#endif
    g_web.onNotFound([]() {                                  // điểm phát riêng: mọi địa chỉ về trang chính
        if (netApOn()) { g_web.sendHeader("Location", String("http://") + WiFi.softAPIP().toString()); g_web.send(302); }
        else g_web.send(404, "text/plain", "404");
    });
    static const char *hdrs[] = { "Cookie" };
    g_web.collectHeaders(hdrs, 1);
    }
    g_web.begin();
    g_webOn = true;
}

// Tắt web (khi tắt WiFi hoặc đổi chế độ)
static void webStop() {
    if (!g_webOn) { g_webLinked = false; return; }
    if (g_dnsOn) { g_dns.stop(); g_dnsOn = false; }
    if (g_otaBusy && !g_clTaskBusy) Update.abort();          // đang nạp dở: bỏ
    if (!g_clTaskBusy) g_updating = false;
    g_web.close();
    g_webOn = false;
    g_otaBusy = false;
    g_webLinked = false;
    g_pinTok[0] = 0;                                         // hết phiên PIN
}

static void webTick() {
    if (!webWanted || !netRadioOn()) { webStop(); return; }  // chỉ bật web ở chế độ 3
    if (webRoastBusy() && !g_clTaskBusy) { webStop(); return; }   // đang cấp nhiệt: tắt web nhường RAM
    if (!g_webOn) {
        if (netOnline() || netApOn()) webStart();
        return;
    }
    if (netApOn() && !g_dnsOn) { g_dns.start(53, "*", WiFi.softAPIP()); g_dnsOn = true; }
    else if (!netApOn() && g_dnsOn) { g_dns.stop(); g_dnsOn = false; }
    if (g_dnsOn) g_dns.processNextRequest();
    g_web.handleClient();
#if CLOUD_ON
    cloudTick();                                             // kiểm tra bản mới trên Cloud (không tự cài)
#endif

    // Chế độ 3 "đã nối" (đèn đỏ đứng): có máy nối vào điểm phát,
    // vừa mở trang web, hoặc đang nạp firmware
    const bool apConnected  = netApOn() && (WiFi.softAPgetStationNum() > 0);
    const bool httpRecent   = (g_netLastUse != 0) && ((millis() - g_netLastUse) < WEB_ACTIVE_HOLD_MS);
    g_webLinked = apConnected || httpRecent || g_otaBusy;
}
