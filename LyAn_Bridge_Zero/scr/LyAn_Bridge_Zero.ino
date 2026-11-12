/*
 * LyAn Bridge Zero - cầu nối máy rang Skywalker V1 với HiBean (Bluetooth) và Artisan (WiFi).
 * Board: Waveshare ESP32-S3-Zero. Partition Scheme: Minimal SPIFFS (1.9MB APP with OTA). PSRAM: Disabled.
 * Thư viện: NimBLE-Arduino, WebSockets (Markus Sattler), PID (br3ttb).
 *
 * Nút BOOT đổi chế độ, đèn RGB báo chế độ (chớp = chờ nối, sáng đứng = đã nối):
 *   1. HiBean  (xanh lá)    - khi mới bật nguồn
 *   2. Artisan (xanh dương) - nhấn BOOT 1 lần
 *   3. Web     (đỏ)         - nhấn BOOT 2 lần: cài WiFi, nạp firmware
 */
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>

#include "RoasterSetting.h"
#include "lib/RoasterState.h"
#include "lib/RoasterPID.h"
#include "lib/RoasterControl.h"
#include "lib/RoasterMachine.h"
#include "lib/RoasterDialect.h"
#include "lib/RoasterRadio.h"
#include "lib/RoasterRGB.h"
#include "lib/RoasterBLE.h"
#include "lib/RoasterNet.h"
#include "lib/RoasterArtisan.h"
#include "lib/RoasterWeb.h"

// Áp dụng chế độ: chỉ bật phần của chế độ đó, tắt phần còn lại.
static void applyWorkMode(uint8_t mode) {
    static uint8_t applied = 0xFF;
    if (mode == applied) return;
    if (g_otaBusy && mode != MODE_WIFI_CONFIG) return;       // Đang nạp firmware OTA dở dang: giữ nguyên chế độ Web

    applied    = mode;
    g_workMode = mode;

    if (mode == MODE_BLE_HIBEAN) {
        // Chế độ 1: HiBean
        artisanWanted = false;
        webWanted     = false;
        netSetEnabled(false);
        netSetAp(false);
        hibeanWanted  = true;
        DLOG("[MODE] -> 1. BLE HIBEAN (Den Xanh La chop -> ket noi dung den)\n");
    }
    else if (mode == MODE_ARTISAN_WS) {
        // Chế độ 2: Artisan
        hibeanWanted  = false;
        webWanted     = false;
        g_webLinked   = false;
        artisanWanted = true;
        netSetAp(false);                                     // nối mạng đã lưu, không có thì tự phát
        netSetEnabled(true);
        DLOG("[MODE] -> 2. ARTISAN WIFI (Den Xanh Duong chop -> ket noi dung den)\n");
    }
    else {
        // Chế độ 3: Web
        hibeanWanted    = false;
        artisanWanted   = false;
        g_artisanLinked = false;
        webWanted       = true;
        netSetAp(false);                                     // nối mạng đã lưu, không có thì tự phát
        netSetEnabled(true);
        DLOG("[MODE] -> 3. WIFI WEB CONFIG (Den Do chop -> ket noi dung den)\n");
    }
}

// Nhịp 100 ms ở core 0: mạng, web, Artisan
static void tickTaskFn(void *) {
    for (;;) {
        applyWorkMode(g_workMode);
        netTick();
        webTick();
        artisanTick();
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

void setup() {
#if SERIAL_DEBUG
    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) delay(10);     // chờ mở Serial Monitor
    DLOG("%s %s (Lite)\n", DEVICE_NAME, FW_VERSION);
#endif
    syncInit();                                              // phải gọi đầu tiên
    bootButtonInit();
    rgbInit();
    controlInit();                                           // mọi thứ tắt
    netInit();
    webPinLoad();                                            // đọc mã PIN web
    bleBegin();
    applyWorkMode(MODE_BLE_HIBEAN);                          // bắt đầu ở chế độ 1
    xTaskCreatePinnedToCore(tickTaskFn, "tick", TICK_TASK_STACK, nullptr, 1, nullptr, 0);
}

void loop() {
    // Nhấn BOOT: đổi sang chế độ kế tiếp
    if (bootButtonPressed() && !g_otaBusy) {
        if (factoryArmed()) webFactoryDo();                  // đang chờ xác nhận Reset Factory
        else {
            uint8_t nextMode = (g_workMode + 1) % 3;
            g_workMode = nextMode;
        }
    }

    rgbTick();
    bleService();
    machinePoll();                    // nhận nhiệt độ từ máy
    rorUpdate();
    dialectDrain();
    safetyWatchdog();
    handlePIDControl();

#if SERIAL_DEBUG
    static uint32_t lastHb = 0;
    if (millis() - lastHb >= 10000) {
        lastHb = millis();
        Snap s;
        roasterSnapshot(s);
        DLOG("[HB] %lus | Mode: %s | RAM %uKB | BT %.1f (%s) Heater %d Air %d Drum %d Cool %d PID %d | HB:%d ART:%d WEB:%d | WiFi:%d (%s - %s)\n",
             (unsigned long)(millis() / 1000UL), workModeName(g_workMode), (unsigned)(ESP.getFreeHeap() / 1024UL),
             s.temp, s.tempOk ? "ok" : "mat tin hieu",
             s.heat, s.vent, s.drum, s.cool, s.pid,
             (int)g_hibeanLinked, (int)g_artisanLinked, (int)g_webLinked,
             (int)wifiRadioUp, netSSID().c_str(), netIP().c_str());
    }
#endif

    // Gửi khung lệnh xuống máy
    const bool sent = machineSend();
    delay(sent ? 1 : 5);
}
