/*
 * GPL v3
 * Cập nhật firmware từ Cloud (Firebase Storage).
 * Ở chế độ Web và có internet: đọc manifest.json, có bản mới thì trang web báo.
 * Không tự cài: người dùng nhập PIN rồi bấm cài.
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"

#if ENABLE_CLOUD_OTA && ENABLE_OTA_WEB
#define CLOUD_ON 1
#else
#define CLOUD_ON 0
#endif

#if CLOUD_ON
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <time.h>
#include "RoasterState.h"
#include "RoasterControl.h"
#include "RoasterNet.h"

// Chứng chỉ gốc Google (GTS Root R1, R3, R4)
static const char CLOUD_ROOT_PEM[] PROGMEM =
R"PEM(
-----BEGIN CERTIFICATE-----
MIIFVzCCAz+gAwIBAgINAgPlk28xsBNJiGuiFzANBgkqhkiG9w0BAQwFADBHMQsw
CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU
MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw
MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp
Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUA
A4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo
27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7w
Cl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjw
TcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0Pfybl
qAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaH
szVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8
Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmk
MiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92
wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70p
aDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrN
VjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQID
AQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E
FgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBAJ+qQibb
C5u+/x6Wki4+omVKapi6Ist9wTrYggoGxval3sBOh2Z5ofmmWJyq+bXmYOfg6LEe
QkEzCzc9zolwFcq1JKjPa7XSQCGYzyI0zzvFIoTgxQ6KfF2I5DUkzps+GlQebtuy
h6f88/qBVRRiClmpIgUxPoLW7ttXNLwzldMXG+gnoot7TiYaelpkttGsN/H9oPM4
7HLwEXWdyzRSjeZ2axfG34arJ45JK3VmgRAhpuo+9K4l/3wV3s6MJT/KYnAK9y8J
ZgfIPxz88NtFMN9iiMG1D53Dn0reWVlHxYciNuaCp+0KueIHoI17eko8cdLiA6Ef
MgfdG+RCzgwARWGAtQsgWSl4vflVy2PFPEz0tv/bal8xa5meLMFrUKTX5hgUvYU/
Z6tGn6D/Qqc6f1zLXbBwHSs09dR2CQzreExZBfMzQsNhFRAbd03OIozUhfJFfbdT
6u9AWpQKXCBfTkBdYiJ23//OYb2MI3jSNwLgjt7RETeJ9r/tSQdirpLsQBqvFAnZ
0E6yove+7u7Y/9waLd64NnHi/Hm3lCXRSHNboTXns5lndcEZOitHTtNCjv0xyBZm
2tIMPNuzjsmhDYAPexZ3FL//2wmUspO8IFgV6dtxQ/PeEMMA3KgqlbbC1j+Qa3bb
bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIICCTCCAY6gAwIBAgINAgPluILrIPglJ209ZjAKBggqhkjOPQQDAzBHMQswCQYD
VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG
A1UEAxMLR1RTIFJvb3QgUjMwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw
WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz
IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjMwdjAQBgcqhkjOPQIBBgUrgQQAIgNi
AAQfTzOHMymKoYTey8chWEGJ6ladK0uFxh1MJ7x/JlFyb+Kf1qPKzEUURout736G
jOyxfi//qXGdGIRFBEFVbivqJn+7kAHjSxm65FSWRQmx1WyRRK2EE46ajA2ADDL2
4CejQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW
BBTB8Sa6oC2uhYHP0/EqEr24Cmf9vDAKBggqhkjOPQQDAwNpADBmAjEA9uEglRR7
VKOQFhG/hMjqb2sXnh5GmCCbn9MN2azTL818+FsuVbu/3ZL3pAzcMeGiAjEA/Jdm
ZuVDFhOD3cffL74UOO0BzrEXGhF16b0DjyZ+hOXJYKaV11RZt+cRLInUue4X
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD
VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG
A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw
WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz
IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi
AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi
QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR
HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW
BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D
9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8
p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD
-----END CERTIFICATE-----
)PEM";

// Trạng thái (trang web đọc qua /cloudstat)
enum CloudState : uint8_t {
    CL_IDLE = 0,          // chưa kiểm tra
    CL_CHECKING = 1,      // đang đọc manifest
    CL_UP_TO_DATE = 2,    // đang là bản mới nhất
    CL_NEW = 3,           // có bản mới
    CL_ERR_NET = 4,       // không đọc được (mất mạng / thiếu RAM)
    CL_ERR_FMT = 5,       // manifest sai khuôn
    CL_DOWNLOADING = 6,   // đang tải + ghi
    CL_DONE = 7,          // xong, sắp khởi động lại
    CL_FAIL = 8           // cài lỗi, có thể cài lại
};

static volatile uint8_t  g_clState    = CL_IDLE;
static volatile int      g_clPct      = 0;          // 0..100 khi đang tải
static volatile bool     g_clTaskBusy = false;      // có một luồng cloud đang chạy (kiểm tra hoặc cài)
static const char       *g_clErr      = nullptr;    // mã lỗi để dịch (nullptr = không lỗi)
static uint32_t          g_clNextCheck = 0;         // lần kiểm tra kế tiếp
static char              g_clVer[24]   = "";        // phiên bản trên cloud (chữ trong manifest)
static char              g_clFile[96]  = "";        // tên file .bin
static char              g_clMd5[40]   = "";        // MD5 (có thể rỗng)
static char              g_clNotes[160] = "";       // ghi chú
static uint32_t          g_clSize      = 0;         // dung lượng .bin (byte)

// Tiện ích
// "4.3.1" -> số để so sánh; sai dạng -> 0
static uint32_t clVerNum(const char *s) {
    if (!s) return 0;
    while (*s == ' ' || *s == 'v' || *s == 'V') s++;
    uint32_t part[3] = {0, 0, 0};
    int i = 0;
    bool any = false;
    for (; *s && i < 3; s++) {
        if (*s >= '0' && *s <= '9') {
            any = true;
            part[i] = part[i] * 10 + (uint32_t)(*s - '0');
            if (part[i] > 999) part[i] = 999;
        } else if (*s == '.') {
            i++;
        } else break;
    }
    return any ? part[0] * 1000000UL + part[1] * 1000UL + part[2] : 0;
}

// Lấy "key":"giá trị" trong manifest
static bool clJsonStr(const String &j, const char *key, char *out, size_t n) {
    const String k = String("\"") + key + "\"";
    int p = j.indexOf(k);
    if (p < 0) return false;
    p = j.indexOf(':', p + (int)k.length());
    if (p < 0) return false;
    p++;
    const int L = (int)j.length();
    while (p < L && (j[p] == ' ' || j[p] == '\t' || j[p] == '\r' || j[p] == '\n')) p++;
    if (p >= L || j[p] != '"') return false;
    p++;
    size_t o = 0;
    while (p < L && j[p] != '"' && o + 1 < n) {
        char c = j[p++];
        if (c == '\\' && p < L) c = j[p++];
        out[o++] = c;
    }
    out[o] = 0;
    return true;
}
// Lấy số "key":123
static bool clJsonNum(const String &j, const char *key, uint32_t &out) {
    const String k = String("\"") + key + "\"";
    int p = j.indexOf(k);
    if (p < 0) return false;
    p = j.indexOf(':', p + (int)k.length());
    if (p < 0) return false;
    p++;
    const int L = (int)j.length();
    while (p < L && (j[p] == ' ' || j[p] == '\t' || j[p] == '\r' || j[p] == '\n')) p++;
    if (p >= L || j[p] < '0' || j[p] > '9') return false;
    uint32_t v = 0;
    while (p < L && j[p] >= '0' && j[p] <= '9') { v = v * 10 + (uint32_t)(j[p] - '0'); p++; }
    out = v;
    return true;
}
static bool clSafeFile(const char *f) {            // chỉ cho chữ, số, . _ - và đuôi .bin
    const size_t n = strlen(f);
    if (n < 5 || n > 90) return false;
    for (size_t i = 0; i < n; i++) {
        const char c = f[i];
        if (!(isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-')) return false;
    }
    return strcmp(f + n - 4, ".bin") == 0 && f[0] != '.';
}
static bool clMd5Ok(const char *s) {
    if (strlen(s) != 32) return false;
    for (int i = 0; i < 32; i++) if (!isxdigit((unsigned char)s[i])) return false;
    return true;
}
// Mã hoá đường dẫn cho URL
static String clEncPath(const String &s) {
    String o;
    o.reserve(s.length() + 16);
    for (size_t i = 0; i < s.length(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
        else { char t[4]; snprintf(t, sizeof t, "%%%02X", c); o += t; }
    }
    return o;
}
static String clUrl(const String &file) {
    String u = String("https://firebasestorage.googleapis.com/v0/b/") + CLOUD_BUCKET + "/o/";
    u += clEncPath(String("firmware/") + CLOUD_PRODUCT + "/" + file);
    u += "?alt=media";
    return u;
}
static uint32_t clSlotSize() {
    const esp_partition_t *p = esp_ota_get_next_update_partition(nullptr);
    return p ? p->size : 0;
}
static void clWaitTime(uint32_t ms) {              // chờ giờ mạng (NTP), không có cũng đi tiếp
    const uint32_t t0 = millis();
    while (time(nullptr) < 1700000000L && (millis() - t0) < ms) vTaskDelay(pdMS_TO_TICKS(100));
}
// Mở kết nối HTTPS tới url; trả mã HTTP (<=0 = lỗi kết nối)
static int clGet(HTTPClient &http, WiFiClientSecure &cli, const String &url) {
    cli.setCACert(CLOUD_ROOT_PEM);
    cli.setTimeout(CLOUD_TIMEOUT_MS / 1000);
    http.setConnectTimeout(CLOUD_TIMEOUT_MS);
    http.setTimeout(CLOUD_TIMEOUT_MS);
    http.useHTTP10(true);                          // không dùng chunked: có Content-Length rõ ràng
    http.setReuse(false);
    if (!http.begin(cli, url)) return -1;
    return http.GET();
}

// 1) Kiểm tra manifest.json
static void clCheckRun() {
    g_clErr = nullptr;
    g_clState = CL_CHECKING;
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    clWaitTime(3000);

    uint8_t result = CL_ERR_NET;
    {
        WiFiClientSecure cli;
        HTTPClient http;
        const int code = clGet(http, cli, clUrl("manifest.json"));
        DLOG("[CLOUD] manifest HTTP %d\n", code);
        if (code == 200 && http.getSize() <= 2048) {
            const String body = http.getString();
            char ver[24] = "", file[96] = "", md5[40] = "", notes[160] = "";
            uint32_t size = 0;
            const bool okStr = clJsonStr(body, "version", ver, sizeof ver) && clJsonStr(body, "file", file, sizeof file);
            const bool okNum = clJsonNum(body, "size", size);
            clJsonStr(body, "md5", md5, sizeof md5);          // không bắt buộc
            clJsonStr(body, "notes", notes, sizeof notes);    // không bắt buộc
            const uint32_t slot = clSlotSize();
            if (okStr && okNum && clVerNum(ver) > 0 && clSafeFile(file) && size > 100000UL && (slot == 0 || size <= slot) &&
                (md5[0] == 0 || clMd5Ok(md5))) {
                snprintf(g_clFile, sizeof g_clFile, "%s", file);
                snprintf(g_clMd5, sizeof g_clMd5, "%s", md5);
                snprintf(g_clNotes, sizeof g_clNotes, "%s", notes);
                snprintf(g_clVer, sizeof g_clVer, "%s", ver);
                g_clSize = size;
                result = (clVerNum(ver) > clVerNum(FW_VERSION)) ? CL_NEW : CL_UP_TO_DATE;
            } else {
                result = CL_ERR_FMT;
            }
        }
        http.end();
    }
    g_clNextCheck = millis() + ((result == CL_NEW || result == CL_UP_TO_DATE) ? CLOUD_CHECK_MS : CLOUD_RETRY_MS);
    g_clState = result;                            // ghi trạng thái sau cùng
    DLOG("[CLOUD] ket qua kiem tra = %u (cloud %s, dang chay %s)\n", (unsigned)result, g_clVer, FW_VERSION);
}

// 2) Cài: tải .bin, kiểm tra, ghi vào ô trống
static void clInstallRun() {
    g_clErr = nullptr;
    g_clPct = 0;
    g_updating = true;                             // đèn hồng
    g_otaBusy = true;                              // giữ chế độ Web
    netHold(true);                                 // không tự tắt WiFi
    bool ok = false;
    bool begun = false;
    do {
        if (controlBusy()) { g_clErr = "u.e_busy"; break; }
        WiFiClientSecure cli;
        HTTPClient http;
        configTime(0, 0, "pool.ntp.org", "time.google.com");
        clWaitTime(3000);
        const int code = clGet(http, cli, clUrl(String(g_clFile)));
        DLOG("[CLOUD] tai %s HTTP %d\n", g_clFile, code);
        if (code != 200) { g_clErr = "cl.e_http"; http.end(); break; }
        const int len = http.getSize();
        if (len > 0 && (uint32_t)len != g_clSize) { g_clErr = "cl.e_size"; http.end(); break; }
        if (!Update.begin(g_clSize)) { g_clErr = "u.e_open"; http.end(); break; }
        begun = true;
        if (g_clMd5[0]) Update.setMD5(g_clMd5);
        auto *s = http.getStreamPtr();
        uint8_t buf[1024];
        uint32_t got = 0, tLast = millis();
        while (got < g_clSize) {
            if (controlBusy()) { g_clErr = "u.e_busy"; break; }
            const int avail = s->available();
            if (avail <= 0) {
                if (!http.connected() || (millis() - tLast) > 15000UL) { g_clErr = "cl.e_http"; break; }
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            size_t want = (size_t)avail < sizeof buf ? (size_t)avail : sizeof buf;
            if (want > g_clSize - got) want = g_clSize - got;
            const int n = s->readBytes(buf, want);
            if (n <= 0) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }
            if (Update.write(buf, (size_t)n) != (size_t)n) { g_clErr = "u.e_write"; break; }
            got += (uint32_t)n;
            tLast = millis();
            g_clPct = (int)((uint64_t)got * 100ULL / g_clSize);
        }
        http.end();
        if (g_clErr) break;
        if (got != g_clSize) { g_clErr = "cl.e_http"; break; }
        if (!Update.end(true)) { g_clErr = (Update.getError() == UPDATE_ERROR_MD5) ? "cl.e_md5" : "u.e_bad"; break; }
        ok = true;
    } while (0);

    if (!ok && begun) Update.abort();
    g_otaBusy = false;
    netHold(false);
    if (ok) {
        g_clPct = 100;
        g_clState = CL_DONE;
        DLOG("[CLOUD] cai xong %s, khoi dong lai\n", g_clVer);
        vTaskDelay(pdMS_TO_TICKS(2500));           // chờ trang web kịp thấy "xong"
        ESP.restart();
    } else {
        g_updating = false;
        g_clState = CL_FAIL;
        DLOG("[CLOUD] cai loi: %s\n", g_clErr ? g_clErr : "?");
    }
}

// Luồng chạy nền
static void clTaskFn(void *arg) {
    if ((int)(intptr_t)arg == 1) clInstallRun(); else clCheckRun();
    g_clTaskBusy = false;
    vTaskDelete(nullptr);
}
// job 0 = kiểm tra, 1 = cài. false = đang bận / thiếu RAM
static bool clStartTask(int job) {
    if (g_clTaskBusy) return false;
    if (ESP.getMaxAllocHeap() < CLOUD_MIN_BLOCK) { g_clErr = "cl.e_mem"; return false; }
    g_clTaskBusy = true;
    const uint8_t prev = g_clState;
    if (job == 1) { g_clPct = 0; g_clErr = nullptr; g_clState = CL_DOWNLOADING; }
    if (xTaskCreatePinnedToCore(clTaskFn, "cloud", CLOUD_TASK_STACK, (void *)(intptr_t)job, 1, nullptr, 0) != pdPASS) {
        g_clTaskBusy = false;
        g_clState = prev;
        g_clErr = "cl.e_mem";
        return false;
    }
    return true;
}
// Người dùng bấm "Kiểm tra lại"
static void cloudRecheckNow() {
    if (g_clTaskBusy || g_otaBusy) return;
    if (!netOnline()) { g_clErr = nullptr; g_clState = CL_ERR_NET; return; }
    g_clErr = nullptr;
    g_clState = CL_CHECKING;
    g_clNextCheck = millis();
}
// Gọi mỗi 100 ms khi web đang mở
static void cloudTick() {
    static bool wasOnline = false;
    const uint32_t now = millis();
    const bool online = netOnline();                // đã nối WiFi nhà
    if (online && !wasOnline) g_clNextCheck = now + CLOUD_FIRST_DELAY_MS;   // vừa nối: chờ mạng ổn định
    wasOnline = online;
    if (!online || g_clTaskBusy || g_otaBusy || controlBusy()) return;
    if ((int32_t)(now - g_clNextCheck) < 0) return;
    if (ESP.getMaxAllocHeap() < CLOUD_MIN_BLOCK) {  // thiếu RAM: để sau
        g_clNextCheck = now + 30000UL;
        if (g_clState == CL_CHECKING) { g_clErr = "cl.e_mem"; g_clState = CL_ERR_NET; }
        return;
    }
    g_clNextCheck = now + CLOUD_RETRY_MS;
    clStartTask(0);
}
#else
static const bool g_clTaskBusy = false;     // tắt Cloud OTA
#endif
