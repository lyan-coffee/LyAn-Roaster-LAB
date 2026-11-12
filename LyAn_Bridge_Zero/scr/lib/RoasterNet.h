/*
 * GPL v3
 * WiFi: nối mạng đã lưu có sóng mạnh nhất, không có thì tự phát điểm WiFi riêng.
 * Lưu tối đa 5 mạng trong bộ nhớ (NVS).
 */
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include "../RoasterSetting.h"
#include "RoasterState.h"
#include "RoasterRadio.h"

enum : uint8_t { NET_OFF = 0, NET_CONNECTING = 1, NET_ONLINE = 2, NET_AP_ONLY = 3 };
enum : uint8_t { NSCAN_IDLE = 0, NSCAN_RUN = 1, NSCAN_DONE = 2 };

struct NetSaved { char ssid[33]; char pass[65]; };

static NetSaved g_netList[WIFI_MAX_NETS];
static uint8_t  g_netCount    = 0;
static bool     g_netWant     = false;          // bật WiFi nối mạng (STA)
static bool     g_netApWant   = false;          // yêu cầu cưỡng bức bật điểm phát (SoftAP)
static bool     g_netApActive = false;          // điểm phát thực tế đang bật
static bool     g_netRadio    = false;          // driver WiFi đã bật chưa
static bool     g_mdnsOk      = false;
static uint8_t  g_netMode     = NET_OFF;
static uint32_t g_netT0       = 0;              // mốc bắt đầu thử nối
static uint32_t g_netNextTry  = 0;              // lần thử lại kế tiếp
static uint8_t  g_scanState   = NSCAN_IDLE;
static int      g_scanN       = 0;
static int      g_lastTryIdx  = -1;             // chỉ số mạng vừa thử kết nối

#define NET_SCAN_FRESH_MS  15000                // kết quả quét còn "mới" trong khoảng này
#define NET_SCAN_TMO_MS    8000                 // một lượt quét chạy quá lâu thì bỏ
static uint32_t g_scanT0     = 0;               // lúc bắt đầu lượt quét
static uint32_t g_scanEndMs  = 0;               // lúc quét xong
static uint8_t  g_scanTry    = 0;               // số lần thử lại khi lệnh quét báo lỗi
static bool     g_scanMissed = false;           // không thấy mạng đã lưu -> lần sau quét lại
static uint32_t g_radioT0    = 0;               // lúc bật radio
static uint8_t  g_netMiss    = 0;               // số lần liên tiếp không thấy mạng đã lưu

// Đóng web và Artisan trước khi tắt WiFi
static void artisanStop();
static void webStop();

// Lưu / đọc mạng WiFi (NVS)
static void netSave() {
    Preferences p;
    if (!p.begin("lynet", false)) return;
    p.putUChar("n", g_netCount);
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
    g_netCount = p.getUChar("n", 0);
    if (g_netCount > WIFI_MAX_NETS) g_netCount = WIFI_MAX_NETS;
    char k[8];
    for (uint8_t i = 0; i < g_netCount; i++) {
        snprintf(k, sizeof k, "s%u", i); p.getString(k, g_netList[i].ssid, sizeof g_netList[i].ssid);
        snprintf(k, sizeof k, "p%u", i); p.getString(k, g_netList[i].pass, sizeof g_netList[i].pass);
    }
    p.end();
    DLOG("[NET] Da doc NVS: %u mang WiFi da luu\n", (unsigned)g_netCount);
}

// Đọc trạng thái
static inline bool     netOnline()    { return g_netMode == NET_ONLINE; }
static inline bool     netApOn()      { return g_netApActive && g_netRadio; }
static inline bool     netRadioOn()   { return g_netRadio; }
static bool            g_netHold    = false;    // đang nạp firmware qua web
static uint32_t        g_netLastUse = 0;        // lần cuối có người dùng mở/tương tác trang web
static inline void     netTouch()     { g_netLastUse = millis(); }
static inline void     netHold(bool on) { g_netHold = on; netTouch(); }
static inline bool     netHeld()      { return g_netHold; }
static inline uint32_t netFreeRam()   { return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
static inline uint8_t  netSavedCount(){ return g_netCount; }
static inline const char *netSavedSSID(int i) { return (i >= 0 && i < g_netCount) ? g_netList[i].ssid : ""; }
static String netSSID() { return g_netMode == NET_ONLINE ? WiFi.SSID() : String(""); }
static String netIP() {
    if (g_netMode == NET_ONLINE) return WiFi.localIP().toString();
    if (g_netApActive)           return WiFi.softAPIP().toString();
    return String("");
}
// 0 = không có sóng, 1..4 = số vạch
static int netBars(int rssi) {
    if (rssi >= -55) return 4;
    if (rssi >= -67) return 3;
    if (rssi >= -78) return 2;
    if (rssi <  -90) return 0;
    return 1;
}

// Điểm phát riêng (SoftAP)
// Chỉ đổi chế độ WiFi khi thật sự cần, để không cắt ngang lúc đang nối
static void netApply(bool force = false) {
    if (!g_netRadio) return;
    wifi_mode_t targetMode = WIFI_OFF;
    if (g_netApActive)  targetMode = WIFI_AP_STA;
    else if (g_netWant) targetMode = WIFI_STA;

    static wifi_mode_t s_curMode = WIFI_OFF;
    static bool        s_apRunning = false;

    if (force) {
        s_curMode = WIFI_OFF;
        s_apRunning = false;
    }

    if (targetMode == WIFI_OFF) {
        if (s_apRunning) { WiFi.softAPdisconnect(true); s_apRunning = false; }
        WiFi.mode(WIFI_OFF);
        s_curMode = WIFI_OFF;
        g_netMode = NET_OFF;
        g_mdnsOk  = false;
        return;
    }

    if (WiFi.getMode() != targetMode || s_curMode != targetMode) {
        WiFi.mode(targetMode);
        s_curMode = targetMode;
        delay(20);
    }

    WiFi.setSleep(false);                                    // tắt tiết kiệm điện để phản hồi nhanh

    if (g_netApActive && !s_apRunning) {
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
        s_apRunning = true;
        DLOG("[NET] Bat diem tu phat SoftAP: %s (%s)\n", WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
    } else if (!g_netApActive && s_apRunning) {
        WiFi.softAPdisconnect(true);
        s_apRunning = false;
        DLOG("[NET] Da tat diem tu phat SoftAP (giu STA ket noi)\n");
    }

    if (!g_netWant && g_netApActive && g_netMode != NET_ONLINE) {
        g_netMode = NET_AP_ONLY;
    }
}

// Bật điểm phát riêng khi không nối được mạng đã lưu
static void netFallbackToAp() {
    if (!g_netApActive) {
        g_netApActive = true;
        netApply();
    }
    if (g_netMode != NET_ONLINE) g_netMode = NET_AP_ONLY;
}

// Quét mạng: một lượt quét nền cho cả 13 kênh (~1,5 giây)
static void netScanKick() {
    WiFi.scanDelete();
    WiFi.scanNetworks(true, false, false, 120);
    g_scanT0 = millis();
}

static void netScanStart() {
    if (!g_netRadio || g_scanState == NSCAN_RUN) return;
    // Quét cần chế độ STA: bật thêm STA, giữ điểm phát
    if ((WiFi.getMode() & WIFI_MODE_STA) == 0) WiFi.mode(g_netApActive ? WIFI_AP_STA : WIFI_STA);
    g_scanN      = 0;
    g_scanMissed = false;
    g_scanTry    = 0;
    netTouch();
    netScanKick();
    g_scanState = NSCAN_RUN;
    DLOG("[NET] Quet mang WiFi (13 kenh)...\n");
}

// Gọi mỗi 100 ms từ netTick(): đợi lượt quét xong
static void netScanPoll(uint32_t now) {
    if (g_scanState != NSCAN_RUN) return;
    const int r = WiFi.scanComplete();
    if (r >= 0) {
        g_scanN = r; g_scanState = NSCAN_DONE; g_scanEndMs = now;
        DLOG("[NET] Quet xong: tim thay %d mang\n", r);
    } else if (r == WIFI_SCAN_FAILED) {                      // lệnh quét không chạy được (radio vừa bật): thử lại vài lần
        if (g_scanTry < 3) { g_scanTry++; netScanKick(); return; }
        g_scanN = 0; g_scanState = NSCAN_DONE; g_scanEndMs = now;
        DLOG("[NET] Quet loi WIFI_SCAN_FAILED\n");
    } else if (now - g_scanT0 > NET_SCAN_TMO_MS) {           // quá lâu: bỏ lượt này
        WiFi.scanDelete();
        g_scanN = 0; g_scanState = NSCAN_DONE; g_scanEndMs = now;
        DLOG("[NET] Quet qua lau -> bo\n");
    }
}
static inline uint8_t netScanState() { return g_scanState; }
static inline int     netScanCount() { return g_scanN; }
static String  netScanSSID(int i)    { return (i >= 0 && i < g_scanN) ? WiFi.SSID(i) : String(""); }
static int     netScanRSSI(int i)    { return (i >= 0 && i < g_scanN) ? WiFi.RSSI(i) : -100; }
static bool    netScanLocked(int i)  { return (i >= 0 && i < g_scanN) ? (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) : true; }
static int netFindSaved(const char *ssid) {
    for (uint8_t i = 0; i < g_netCount; i++) if (strcmp(g_netList[i].ssid, ssid) == 0) return i;
    return -1;
}

// Nối / ngắt
static bool netRadioStart();

static void netTryConnect(int idx) {
    if (idx < 0 || idx >= g_netCount) return;
    if (g_scanState == NSCAN_RUN) {                          // đang quét mà phải nối (người dùng bấm Lưu): bỏ lượt quét
        WiFi.scanDelete();
        g_scanState = NSCAN_IDLE;
    }
    g_lastTryIdx = idx;
    g_netT0      = millis();
    g_netMode    = NET_CONNECTING;
    DLOG("[NET] Dang ket noi vao mang da luu [%d]: '%s'\n", idx, g_netList[idx].ssid);
    WiFi.begin(g_netList[idx].ssid, g_netList[idx].pass);
}

// Chọn mạng đã lưu có sóng mạnh nhất
static int netPickBest() {
    int best = -1, bestR = -1000;
    for (int i = 0; i < g_scanN; i++) {
        const int k = netFindSaved(WiFi.SSID(i).c_str());
        if (k < 0) continue;
        const int r = WiFi.RSSI(i);
        if (r > bestR) { bestR = r; best = k; }
    }
    return best;
}

// Lưu mạng mới (hoặc đổi mật khẩu) rồi nối ngay
static bool netJoin(const char *ssid, const char *pass) {
    if (!ssid || !ssid[0]) return false;
    if (!g_netRadio && !netRadioStart()) return false;
    int k = netFindSaved(ssid);
    if (k >= 0) {
        // Đưa mạng vừa chọn xuống vị trí cuối cùng (mới nhất)
        for (uint8_t i = k; i + 1 < g_netCount; i++) g_netList[i] = g_netList[i + 1];
        k = g_netCount - 1;
    } else {
        if (g_netCount >= WIFI_MAX_NETS) {
            for (uint8_t i = 0; i + 1 < g_netCount; i++) g_netList[i] = g_netList[i + 1];
            g_netCount--;
        }
        k = g_netCount++;
    }
    strncpy(g_netList[k].ssid, ssid, sizeof g_netList[k].ssid - 1); g_netList[k].ssid[sizeof g_netList[k].ssid - 1] = 0;
    strncpy(g_netList[k].pass, pass ? pass : "", sizeof g_netList[k].pass - 1); g_netList[k].pass[sizeof g_netList[k].pass - 1] = 0;
    g_netWant = true;
    g_netMiss = 0;
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
    netSave();
    if (cur) { WiFi.disconnect(false, false); g_netMode = NET_OFF; g_netNextTry = millis(); }
}

// Bật / tắt WiFi
static bool netRadioStart() {
    if (g_netRadio) return true;
    if (millis() < NET_BOOT_DELAY_MS) return false;
    // Chờ BLE tắt hẳn rồi mới bật WiFi (tránh thiếu RAM)
    if (hibeanWanted || bleStackUp) return false;
    if (netFreeRam() < NET_MIN_HEAP) return false;

    netLoad();                                               // đọc lại danh sách mạng đã lưu
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.setHostname(WIFI_HOSTNAME);
    g_netRadio   = true;
    wifiRadioUp  = true;
    g_radioT0    = millis();
    g_netMiss    = 0;
    g_lastTryIdx = -1;

    // Có mạng đã lưu: nối trước; chưa có: tự phát ngay
    if (g_netCount > 0 && g_netWant && !g_netApWant) {
        g_netApActive = false;
    } else {
        g_netApActive = true;
    }
    netApply(true);
    g_netNextTry = 0;

    if (g_netCount == 1 && g_netWant) {
        // Chỉ 1 mạng đã lưu: nối thẳng
        netTryConnect(0);
    } else if (g_netCount > 1 && g_netWant) {
        // Nhiều mạng: quét để chọn sóng mạnh nhất
        netScanStart();
    }
    return true;
}

static void netRadioStop() {
    if (!g_netRadio) return;
    artisanStop();                                           // đóng web và Artisan trước
    webStop();
    if (g_mdnsOk) { MDNS.end(); g_mdnsOk = false; }
    // eraseap=false: giữ cấu hình để lần sau nối nhanh
    WiFi.disconnect(true, false);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    g_netRadio    = false;
    wifiRadioUp   = false;
    g_netApActive = false;
    g_netMode     = NET_OFF;
    g_scanState   = NSCAN_IDLE;
    g_scanN       = 0;
    g_netMiss     = 0;
}

static void netSetEnabled(bool on) {
    if (g_netWant == on) return;
    g_netWant = on;
    if (on) {
        if (g_netRadio) netApply();
        else netRadioStart();
        g_netNextTry = 0;
    } else if (!g_netApWant) {
        netRadioStop();
    } else {
        WiFi.disconnect(true, false);
        g_netApActive = true;
        g_netMode = NET_AP_ONLY;
        g_mdnsOk = false;
        netApply();
    }
}

static void netSetAp(bool on) {
    if (g_netApWant == on) return;
    g_netApWant = on;
    if (on) {
        g_netApActive = true;
        if (g_netRadio) netApply();
        else netRadioStart();
    } else if (!g_netWant) {
        netRadioStop();
    } else {
        if (g_netMode == NET_ONLINE) {
            g_netApActive = false;
            netApply();
        }
    }
}

// Chỉ đọc danh sách mạng, chưa bật WiFi
static void netInit() {
    netLoad();
    g_netNextTry = 0;
}

// Gọi mỗi 100 ms (core 0)
static void netTick() {
    const uint32_t now = millis();

    if (g_netWant || g_netApWant) {
        if (!g_netRadio && !netRadioStart()) return;
    } else {
        if (g_netRadio) netRadioStop();
        return;
    }

    // Chờ quét xong
    netScanPoll(now);

    if (!g_netWant) return;

    // Đã nối mạng
    if (WiFi.status() == WL_CONNECTED) {
        if (g_netMode != NET_ONLINE) {
            g_netMode = NET_ONLINE;
            g_netMiss = 0;
            DLOG("[NET] Da ket noi mang '%s' (IP: %s, RSSI: %d dBm)\n",
                 WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
            // Không cần điểm phát và không ai nối vào: tắt điểm phát
            if (!g_netApWant && g_netApActive && WiFi.softAPgetStationNum() == 0) {
                g_netApActive = false;
                netApply();
            }
            if (!g_mdnsOk) g_mdnsOk = MDNS.begin(WIFI_HOSTNAME);
            if (g_mdnsOk) MDNS.addService("http", "tcp", WEB_SERVER_PORT);
        }
        return;
    }

    // Đang nối
    if (g_netMode == NET_CONNECTING) {
        if (now - g_netT0 < WIFI_CONNECT_TMO_MS) return;
        WiFi.disconnect(false, false);
        DLOG("[NET] Ket noi qua %d ms khong thanh cong -> Bat diem tu phat SoftAP\n", WIFI_CONNECT_TMO_MS);
        netFallbackToAp();
        g_netNextTry = now + 6000;
        return;
    }

    // Chưa lưu mạng nào: tự phát ngay
    if (g_netCount == 0) {
        netFallbackToAp();
        return;
    }

    if (now < g_netNextTry) return;

    // Có người đang nối vào điểm phát: không tự quét / tự nối
    if (g_netApActive && WiFi.softAPgetStationNum() > 0) return;

    // Chỉ 1 mạng đã lưu: nối thẳng
    if (g_netCount == 1) {
        netTryConnect(0);
        return;
    }

    // Nhiều mạng: xem kết quả quét, chọn sóng mạnh nhất
    if (g_scanState == NSCAN_RUN) return;
    if (g_scanState != NSCAN_DONE || g_scanMissed || (now - g_scanEndMs) > NET_SCAN_FRESH_MS) {
        netScanStart();
        return;
    }

    const int best = netPickBest();
    if (best >= 0) {
        g_netMiss = 0;
        netTryConnect(best);
    } else {
        // Quét không thấy (mạng ẩn / phát chậm): thử nối mạng mới nhất một lần
        if (g_netMiss == 0 && g_netCount > 0) {
            g_netMiss++;
            g_scanMissed = true;
            netTryConnect(g_netCount - 1);
            return;
        }
        DLOG("[NET] Khong tim thay mang da luu -> Bat diem tu phat SoftAP (%s)\n", WIFI_AP_SSID);
        netFallbackToAp();
        g_netNextTry = now + (g_netMiss < WIFI_FAST_RETRIES ? WIFI_FAST_RETRY_MS : WIFI_RETRY_MS);
        if (g_netMiss < 250) g_netMiss++;
        g_scanMissed = true;
    }
}
