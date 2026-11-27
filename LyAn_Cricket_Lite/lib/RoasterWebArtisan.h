/*
 * lib/RoasterWebArtisan.h - Artisan web pages: /artisan, /artisan.aset (tier 6)
 *
 * The web core does not know Artisan exists:
 * this file attaches itself via webAddNav() ("Artisan setup" button on the home page) and webAddRoutes() (two paths); call webArtisanInit() once in setup().
 *
 *   /artisan        ws://<IP>:81/ address, settings file download, steps after flashing, control button row
 *   /artisan.aset   Artisan settings file (.aset) embedded in firmware (lib/RoasterAsetData.h); only host= and port= of [WebSocket] are replaced when sent
 *
 * The embedded file is always sent (no user-edited file from FFat); the "ar.dev" row is not shown.
 * i18n keys "nav.artisan", "ar.*" are in lib/RoasterWebI18n.h (EN | VI | 中文).
 * ----------------------------------------------------------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterWeb.h"
#include "RoasterArtisan.h"

#if HAS_MODULE_WEB && ENABLE_ARTISAN_WS
#define HAS_MODULE_WEB_ARTISAN 1

#include "RoasterAsetData.h"

// Artisan settings file (.aset): same as the user's reference "LyGia Roaster v1.1.aset" (curve colors, line styles,
// control buttons, sliders, Heater/Air extra devices, WebSocket...).
//   - Embedded in firmware (RoasterAsetData.h, ~49 KB flash).
//   - Only 2 values are replaced when sending: host= and port= in [WebSocket] (192.168.4.1 via the LyAn access point,
//     DHCP IP on home WiFi).
//   - Streamed from flash in chunks, no large String, no RAM cost.
//   Mode=C in the file is REQUIRED (Artisan rejects a file without it).
static void webAsetPart(PGM_P p, size_t n) {
    while (n) {
        const size_t k = n > 1024 ? 1024 : n;
        g_web.sendContent_P(p, k);
        p += k;
        n -= k;
    }
}

static void webAset() {
    netTouch();
    const String ip   = netOnline() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
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
    const String ip = netOnline() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    String b = "<div class=c><div class=m>" + wt("ar.addr") + "</div><div class=v>ws://" + ip + ":" + String(ARTISAN_WS_PORT) + "/</div></div>"
               "<div class=c><a class='b p' href='/artisan.aset'>" + wt("ar.dl") + "</a>"
               "<div class='m s'>" + wt("ar.dlnote") + "</div>";
    b += "</div>"
         "<div class=c><div class=m>" + wt("ar.after") + "</div><div class='m s'>" + wt("ar.steps") + "</div></div>"
         "<div class=c><div class=m>" + wt("ar.btns") + "</div><div class='m s'>" + wt("ar.btnstxt") + "</div></div>";
    g_web.send(200, "text/html", webPage(wt("nav.artisan"), b));
}

// Called when the web starts (webAddRoutes): registers the two paths
static void webArtisanRoutes() {
    webOnG("/artisan", webArtisan);
    webOnG("/artisan.aset", webAset);
}

// Call once in setup(), BEFORE the web starts (the web starts only after WiFi is up)
static void webArtisanInit() {
    static bool done = false;
    if (done) return;
    done = true;
    webAddNav("nav.artisan", "/artisan");
    webAddRoutes(webArtisanRoutes);
}

#else
static inline void webArtisanInit() {}
#endif
