/*
 * Cấu hình LyAn Bridge Zero. Mọi thông số đặt ở đây, sửa xong build và nạp lại.
 */
#pragma once

#define FW_VERSION   "4.3.1"
#define DEVICE_NAME  "LyAn Roaster Zero"      // HiBean hiện: tên + phiên bản
#define BLE_NAME     "LyAn Roaster Zero"      // tên Bluetooth (tối đa 26 ký tự)

// 1. SERIAL DEBUG
// 1 = in log ra USB, KHÔNG nối được máy rang (cổng USB dùng cho Serial).
// 0 = dùng thật: cổng USB nối máy rang. Nạp lại firmware qua web.
#define SERIAL_DEBUG 0

// 2. Chân nối máy rang (trùng chân USB)
#if SERIAL_DEBUG
  #define ROASTER_TX_PIN -1
  #define ROASTER_RX_PIN -1
#else
  #define ROASTER_TX_PIN 19                // phát lệnh xuống máy
  #define ROASTER_RX_PIN 20                // nhận nhiệt độ từ máy
#endif
#define ROASTER_BOOT_DELAY_MS 3000         // chờ 3 giây rồi mới chiếm chân USB

// 3. AN TOÀN
// Heater bị cắt khi: dừng khẩn, quá nhiệt, mất tín hiệu nhiệt độ.
// Bật Heater thì Air tự lên tối thiểu MIN_AIR_FOR_HEAT.
// BT >= DRUM_AUTO_ON_C thì Drum tự bật, không cho tắt.
#define MIN_AIR_FOR_HEAT   10              // Air tối thiểu (%) khi bật Heater
#define HEAT_REQUIRES_TEMP_SIGNAL 0        // 1 = chưa có tín hiệu nhiệt thì không cho bật Heater
#define DRUM_AUTO_ON_C     50              // BT từ mức này: Drum tự bật
#define OVERHEAT_CUT_C     240             // BT từ mức này: cắt Heater (0 = tắt luật này)
#define TEMP_STALE_MS      7500            // mất tín hiệu quá lâu: cắt Heater, tắt PID
#define TEMP_SIGNAL_OK_MS  3000            // có nhiệt độ trong khoảng này = có tín hiệu
#define HIBEAN_TIMEOUT_MS  10000           // HiBean im quá lâu: PID chuyển sang tay
#define ROR_WINDOW_S       30              // cửa sổ tính RoR (giây)

// 4. PID
#define DEFAULT_PID_SETPOINT  175          // nhiệt độ Set mặc định (°C)
#define PID_DEFAULT_KP        18.0
#define PID_DEFAULT_KI        0.3
#define PID_DEFAULT_KD        30.0
#define PID_DEFAULT_PMODE     1            // 1 = P_ON_E, 0 = P_ON_M
// Hỗ trợ tăng nhiệt nhanh và chống vượt Set
#define PID_BOOST_FAR_C       10           // còn cách Set hơn (°C): Heater ít nhất PID_BOOST_FAR_PCT
#define PID_BOOST_FAR_PCT     95
#define PID_BOOST_NEAR_C      5            // còn cách Set hơn (°C): Heater ít nhất PID_BOOST_NEAR_PCT
#define PID_BOOST_NEAR_PCT    75
#define PID_BOOST_END_C       2            // thôi hỗ trợ khi cách Set dưới (°C)
#define PID_LEAD_S            10           // nhìn trước BT sau ngần này giây
#define PID_OVER_SOFT_C       1            // vượt Set hơn (°C): Heater tối đa PID_OVER_SOFT_PCT
#define PID_OVER_SOFT_PCT     10
#define PID_OVER_MAX_C        3            // vượt Set hơn (°C): cắt Heater
#define PID_I_BAND_C          8            // cách Set xa hơn (°C) thì xoá tích phân
#define PID_I_PRESET          40           // mức Heater (%) nạp sẵn cho tích phân

// 5. ĐÈN RGB VÀ NÚT BOOT
// Xanh lá = HiBean, Xanh dương = Artisan, Đỏ = Web.
// Chớp = chờ nối, sáng đứng = đã nối. Tím chớp nhanh = mất tín hiệu nhiệt độ.
#define RGB_LED_PIN           21
#define RGB_LED_BRIGHTNESS    60           // độ sáng 0..255
#define RGB_BLINK_MS          500          // chớp chậm (ms)
#define RGB_BLINK_FAST_MS     200          // chớp nhanh (ms)
#define RGB_LED_SWAP_RG       1            // màu đỏ và xanh lá bị đảo thì đổi 0 <-> 1

#define BOOT_BTN_PIN          0            // nút BOOT (nhấn = LOW)
#define BOOT_BTN_DEBOUNCE_MS  45           // chống dội phím (ms)

// 6. WEB: thời gian giữ trạng thái "đã nối"
#define RADIO_AUTO_REJOIN_MS  3000
#define WEB_ACTIVE_HOLD_MS    15000        // giữ đèn đứng 15 giây sau lần mở trang

// 7. MẠNG, WEB, OTA, ARTISAN
#define WIFI_MAX_NETS      5               // số mạng WiFi lưu được
#define WIFI_HOSTNAME      "lyanzero"     // http://lyanzero.local
#define WIFI_AP_SSID       "LyAn_Roaster_Zero"        // điểm phát riêng (http://192.168.4.1)
#define WIFI_AP_PASS       "0918393391"    // tối thiểu 8 ký tự
#define WIFI_CONNECT_TMO_MS 12000          // chờ nối mạng đã lưu (ms) rồi mới tự phát
#define WIFI_RETRY_MS      30000           // không thấy mạng đã lưu: thử lại sau (ms)
#define WIFI_FAST_RETRY_MS 10000           // mấy lần đầu thử lại sớm hơn (ms)
#define WIFI_FAST_RETRIES  3               // số lần thử lại sớm
#define NET_BOOT_DELAY_MS  500             // chờ trước khi bật WiFi (ms)
#define NET_MIN_HEAP       55000           // RAM tối thiểu để bật WiFi (byte)
#define WEB_SERVER_PORT    80
#define ENABLE_OTA_WEB     1               // nạp firmware qua trang web
#define WEB_PIN            "0918393391"    // PIN mặc định. Người dùng đổi được trên web.
#define FACTORY_BOOT_MS    10000           // Reset Factory: thời gian chờ nhấn BOOT xác nhận (ms)
#define ARTISAN_WS_PORT    81
#define ARTISAN_STEP       5               // bước tăng / giảm Heater, Air trên Artisan (%)

// Trang giới thiệu. Viết không dấu. Để "" thì ẩn dòng đó.
#define ABOUT_TAGLINE  "LY AN Roaster LAB"
#define ABOUT_PHONE    "+8491 8393 391"
#define ABOUT_ZALO     ""
#define ABOUT_WEB      ""
#define ABOUT_FACEBOOK ""
#define ABOUT_EMAIL    "lycoffee.ai@gmail.com"
#define ABOUT_ADDRESS  "Ho Chi Minh - Viet Nam"

// 8. CẬP NHẬT FIRMWARE TỪ CLOUD (Firebase Storage)
// Chế độ Web + có internet: tự kiểm tra bản mới, người dùng bấm cài.
#define ENABLE_CLOUD_OTA      1
#define CLOUD_BUCKET          "lyan-ota-server.firebasestorage.app"   // tên kho Firebase Storage
#define CLOUD_PRODUCT         "zero"                                  // thư mục firmware/<tên>/ trong kho
#define CLOUD_CHECK_MS        21600000UL   // kiểm tra lại mỗi 6 giờ
#define CLOUD_RETRY_MS        600000UL     // lỗi: thử lại sau 10 phút
#define CLOUD_FIRST_DELAY_MS  8000         // vừa nối mạng: chờ rồi mới kiểm tra
#define CLOUD_TIMEOUT_MS      10000        // thời gian chờ tối đa (ms)
#define CLOUD_MIN_BLOCK       50000        // RAM liền tối thiểu để mở HTTPS (byte)
#define CLOUD_TASK_STACK      20480        // bộ nhớ ngăn xếp cho luồng Cloud (byte)

// Nhịp 100 ms ở core 0 cho mạng / web / Artisan
#define TICK_MS            100
#define TICK_TASK_STACK    12288

// Debug: DLOG(...) chỉ chạy khi SERIAL_DEBUG = 1
#if SERIAL_DEBUG
  #define DLOG(...) Serial.printf(__VA_ARGS__)
#else
  #define DLOG(...) do {} while (0)
#endif
