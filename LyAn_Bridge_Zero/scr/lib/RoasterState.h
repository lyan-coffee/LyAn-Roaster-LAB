/*
 * GPL v3
 * Trạng thái dùng chung: khoá đa luồng, khung lệnh xuống máy, nhiệt độ, chế độ.
 */
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../RoasterSetting.h"

// Khoá đa luồng
static SemaphoreHandle_t g_mux = nullptr;
inline void syncInit() { if (!g_mux) g_mux = xSemaphoreCreateRecursiveMutex(); }
#define LOCK()   do { if (g_mux) xSemaphoreTakeRecursive(g_mux, portMAX_DELAY); } while (0)
#define UNLOCK() do { if (g_mux) xSemaphoreGiveRecursive(g_mux); } while (0)

// Khung điều khiển 6 byte xuống máy: {VENT, FILTER, COOL, DRUM, HEAT, CHECK}
// Giá trị 0..100. CHECK = tổng 5 byte đầu.
static const int CONTROLLER_LENGTH = 6;
enum ControlBytes { VENT_BYTE = 0, FILTER_BYTE = 1, COOL_BYTE = 2, DRUM_BYTE = 3, HEAT_BYTE = 4, CHECK_BYTE = 5 };
static uint8_t sendBuffer[CONTROLLER_LENGTH] = { 0, 0, 0, 0, 0, 0 };

// Nhiệt độ hạt BT (luôn °C)
static double   temp = 0.0;
static char     CorF = 'C';                    // đơn vị HiBean muốn (C / F)
static uint32_t lastTempMs = 0;                // lần cuối nhận nhiệt độ (0 = chưa từng)

// Đang có tín hiệu nhiệt độ?
static bool tempSignalOk()   { return (lastTempMs != 0) && ((millis() - lastTempMs) < TEMP_SIGNAL_OK_MS); }
// Đã có tín hiệu nhưng giờ mất
static bool tempSignalDropped() { return TEMP_STALE_MS > 0 && lastTempMs != 0 && (millis() - lastTempMs) > TEMP_STALE_MS; }
static bool tempSignalLost() { return TEMP_STALE_MS > 0 && (lastTempMs == 0 || (millis() - lastTempMs) > TEMP_STALE_MS); }

// 3 chế độ, đổi bằng nút BOOT
enum BridgeWorkMode : uint8_t {
    MODE_BLE_HIBEAN  = 0,  // 1: HiBean (BLE)
    MODE_ARTISAN_WS  = 1,  // 2: Artisan (WiFi)
    MODE_WIFI_CONFIG = 2   // 3: Web cài đặt
};

static volatile uint8_t  g_workMode      = MODE_BLE_HIBEAN;
static volatile bool     g_hibeanLinked  = false;   // HiBean đang nối (BLE)
static volatile bool     g_artisanLinked = false;   // Artisan đang nối (WebSocket)
static volatile bool     g_webLinked     = false;   // có người đang dùng trang web
static volatile uint32_t g_lastHiBeanMs  = 0;       // lần cuối nhận lệnh HiBean
static volatile bool     g_updating      = false;   // đang nạp firmware (đèn hồng)
static volatile uint32_t g_factoryUntil  = 0;       // != 0: đang chờ nhấn BOOT xác nhận Reset Factory
static inline bool factoryArmed() { return g_factoryUntil != 0 && (int32_t)(g_factoryUntil - millis()) > 0; }
