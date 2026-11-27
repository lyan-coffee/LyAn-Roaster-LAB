/*
 * RoasterSetting.h - Firmware Configuration (Open-Source Community Edition)
 *
 * Board   : Waveshare ESP32-S3-Touch-LCD-1.54 (ST7789 240x240, CST816S, BOOT/PWR/PLUS, I2S audio, battery)
 * Roaster : Skywalker V1 (default)
 *
 * Copyright (C) 2026 LyAn Roaster LAB (lycoffee.ai@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#define CFG_STR_(x) #x
#define CFG_STR(x)  CFG_STR_(x)

// =============================================================================
// 1. DEVICE IDENTITY
// =============================================================================
#define ROASTER_V3    1
#define FW_VERSION    "v2.0.1-oss"
#define FW_STAGE      "Community Edition (GPLv3)"
#define BLE_NAME      "LyAn Cricket Lite"       // BLE advertised name (max 26 chars)
#define DEVICE_NAME   "LyAn Cricket Lite"       // Display name for HiBean / Web
#define WELCOME_TITLE "Cricket Lite"            // Product title on Welcome screen

// Serial debug logging (requires "USB CDC On Boot = Enabled"). Set 0 when wired to a real roaster.
#define SERIAL_DEBUG          0
#define SERIAL_BAUD           115200
#define SERIAL_WAIT_MS        2500
#define ENABLE_SERIAL_CONSOLE 0

// =============================================================================
// 2. BOARD & ROASTER MACHINE SELECTION
// =============================================================================
#define BOARD_WAVESHARE_S3_TOUCH_LCD_154 1
#define ENABLE_MACHINE_SKYWALKER_V1      1      // Skywalker V1 (electric hot-air roaster)

// =============================================================================
// 3. ROASTER INTERFACE PINS (SKYWALKER V1)
// =============================================================================
#if SERIAL_DEBUG
  #define ROASTER_TX_PIN      -1
  #define ROASTER_RX_PIN      -1
#else
  #define ROASTER_TX_PIN      19                // TX control frame to Skywalker V1
  #define ROASTER_RX_PIN      20                // RX temperature frame from Skywalker V1
#endif
#define ROASTER_BOOT_DELAY_MS 3000              // Delay before initializing roaster GPIO

// =============================================================================
// 4. MODULE FLAGS (1 = enabled, 0 = disabled)
// =============================================================================
#define ENABLE_DISPLAY           1              // ST7789 240x240 LCD + PWM backlight
#define ENABLE_TOUCH             1              // CST816S I2C capacitive touch
#define ENABLE_BUTTONS           1              // Physical buttons: BOOT (0), PWR (5), PLUS (4)
#define ENABLE_BATTERY           1              // Battery ADC (GPIO1), charge (GPIO3), hold (GPIO2)
#define TOUCH_DEBUG              0

#define ENABLE_UI                1              // LVGL 9.2.x 240x240 touch UI
#define ENABLE_UI_PWRMENU        1              // Long-press PWR (5s) for Reset / Power Off menu
#define ENABLE_SETTINGS_PAGE     1              // Settings page + NVS persistence ("lygia")
#define ENABLE_OVERHEAT_ALARM    1              // Cut Heater when BT >= DEFAULT_WARN_TEMP
#define ENABLE_SIGNAL_LOST_ALARM 1              // Cut Heater & PID when temp signal lost > TEMP_STALE_MS
#define ENABLE_SIMULATION        1              // Built-in roaster simulator when no live signal
#define ENABLE_ROAST_GRAPH       1              // Sample BT / RoR per second & detect Turning Point (TP)

#define ENABLE_HIBEAN_BLE        1              // HiBean Bluetooth LE bridge
#define ENABLE_WIFI              1              // Wi-Fi STA + SoftAP + NTP + mDNS
#define ENABLE_WEB_PORTAL        1              // Web Portal (Wi-Fi setup, Artisan .aset, About)
#define ENABLE_ARTISAN_WS        1              // Artisan WebSocket server (port 81)

// Unused 3.5" flags kept disabled on 1.54" square display
#define ENABLE_IDLE_SCREEN       0
#define ENABLE_ABOUT_PAGE        0
#define ENABLE_START_DIALOG      0
#define ENABLE_CN_FONT           0

// =============================================================================
// 5. SAFETY & CONTROL PARAMETERS
// =============================================================================
#define SAFETY_INTERLOCK      1                 // Block/warn heat when Drum OFF, Cool ON, or Air too low
#define MIN_AIR_FOR_HEAT      10                // Minimum Air (%) while heating
#define DRUM_AUTO_ON_C        50                // Auto-enable and lock Drum when BT >= 50 C
#define ALARM_HYST_C          5                 // Overheat clear hysteresis (C)
#define HIBEAN_TIMEOUT_MS     10000             // HiBean disconnect timeout (ms)
#define TEMP_STALE_MS         5000              // Temp signal-lost timeout while heating (ms)
#define TEMP_SIGNAL_OK_MS     3000              // Valid temp frame freshness window (ms)
#define TEMP_SAMPLE_MS        500               // Roaster telemetry sample interval (ms)
#define DROP_CONFIRM_MS       5000              // Drop double-tap confirm window (ms)
#define END_CONFIRM_MS        5000              // Mid-roast End confirm window (ms)
#define ROR_WINDOW_S          30                // OLS linear regression RoR window (s)

// =============================================================================
// 6. STANDARD PID & MANUAL EVENT PRESETS
// =============================================================================
#define ENABLE_PID_SETTINGS   1                 // Editable Kp / Ki / Kd / P-Mode saved in NVS
#define DEFAULT_PID_SETPOINT  175               // Default PID setpoint (C)
#define PID_DEFAULT_KP        12.0              // Defaults match HiBean: Kp 12 / Ki 0.30 / Kd 20, P_ON_E, 500 ms
#define PID_DEFAULT_KI        0.30
#define PID_DEFAULT_KD        20.0
#define PID_SAMPLE_MS         500               // PID compute interval (ms), same as HiBean
#define PID_DEFAULT_PMODE     1                 // 1 = P_ON_E, 0 = P_ON_M
#define PID_I_BAND_C          8                 // Anti-windup integral clamp band (C)
#define PID_I_PRESET          40                // Anti-windup integral preset (%)

#define ENABLE_EVENT_PRESETS  1                 // Auto-apply Heater / Air / PID on PR / CH events
#define PRESET_PR_TEMP_C      180               // PR: enable PID at 180 C
#define PRESET_PR_TEMP_F      356
#define PRESET_PR_AIR         10                // PR: Air 10%
#define PRESET_CH_HEATER      70                // CH: disable PID, Heater 70%
#define PRESET_CH_AIR         20                // CH: Air 20%

// =============================================================================
// 7. HIBEAN, OVERHEAT & SIMULATION DEFAULTS
// =============================================================================
#define HIBEAN_DEFAULT_ON     0
#define HIBEAN_AUTOSTART      1
#define WARN_TEMP_MIN         220
#define WARN_TEMP_MAX         260
#define WARN_TEMP_STEP        5
#define DEFAULT_TEMP_UNIT     0
#define DEFAULT_WARN_TEMP     240               // Default overheat cutoff (C)
#define DEFAULT_SIMULATION    1                 // Auto-enter simulator when no roaster connected
#define SIM_AMBIENT           30.0f
#define SIM_SCALE_DEFAULT     1                 // 1x real-time simulation speed

// =============================================================================
// 8. DISPLAY, BUTTONS & BATTERY TIMING
// =============================================================================
#define TICK_MS               100               // Core 0 service tick (net/web/artisan/battery)
#define TICK_TASK_STACK       12288
#define BUTTON_DEBOUNCE_MS    30
#define BUTTON_LONG_MS        800
#define BACKLIGHT_PCT         60
#define BACKLIGHT_PWM_HZ      5000
#define BATTERY_SAMPLE_MS     1000
#define BATTERY_AVG           16
#define BATTERY_LOG_MS        15000
#define BAT_HOLD_ENABLE       1
#define BAT_POWEROFF_WAIT_MS  800

// =============================================================================
// 9. WI-FI, WEB PORTAL & ARTISAN
// =============================================================================
#define WIFI_MAX_NETS         5
#define WIFI_HOSTNAME         "lycricket"       // http://lycricket.local
#define WIFI_AP_SSID          "AP_Cricket"      // SoftAP SSID (http://192.168.4.1)
#define WIFI_AP_PASS          "12345678"        // Default SoftAP password
#define WIFI_CONNECT_TMO_MS   15000
#define WIFI_RETRY_MS         30000
#define NET_BOOT_DELAY_MS     2000              // Wait for UI init before bringing Wi-Fi up
#define NET_MIN_HEAP          60000
#define WIFI_IDLE_OFF_MS      600000
#define NTP_SERVER1           "pool.ntp.org"
#define NTP_SERVER2           "time.google.com"
#define NTP_TZ                "ICT-7"
#define CLOCK_DEFAULT_TZ_MIN  420               // Default UTC offset in minutes (+420 = UTC+7)
#define WEB_PIN               "1234"            // Default Web Portal PIN (4..10 digits)
#define ARTISAN_WS_PORT       81
#define ARTISAN_STEP          5
#define WEB_SERVER_PORT       80
#define NET_HEADLESS_AUTOSTART 0
#define HEADLESS_UI_READY_MS  1000
#define RADIO_COEXIST         0
#define RADIO_AUTO_DEFAULT    0
#define RADIO_AUTO_REJOIN_MS  3000

// About card info (/about on Web Portal)
#define ABOUT_TAGLINE         "LY AN Roaster LAB"
#define ABOUT_PHONE           ""
#define ABOUT_ZALO            ""
#define ABOUT_WEB             ""
#define ABOUT_FACEBOOK        ""
#define ABOUT_EMAIL           "lycoffee.ai@gmail.com"
#define ABOUT_ADDRESS         "Ho Chi Minh - Viet Nam"

// =============================================================================
// 10. BOARD PINOUT & AUDIO / DEPENDENCY GUARDS
// =============================================================================
#include "boards/RoasterBoard.h"

// Safety alarm & cricket cue audio (lib/RoasterAlarmSound.h + lib/RoasterCue.h)
#define ALARM_AUDIO_MODE      1                 // 0 = off | 1 = I2S codec (ES8311 + NS4150B) | 2 = GPIO buzzer
#define ALARM_VOLUME          26000             // Alarm amplitude (0..32767)
#define ALARM_CODEC_VOL       0xBF              // ES8311 DAC volume (0xBF = 0 dB)
#define ALARM_CODEC_ADDR      0x18              // ES8311 I2C address
#define ALARM_BUZZER_PIN      -1
#if BOARD_HAS_AUDIO
  #define ALARM_PA_PIN        AUDIO_PA_CTRL_PIN
  #define ALARM_I2S_MCLK      AUDIO_I2S_MCLK
  #define ALARM_I2S_BCLK      AUDIO_I2S_BCLK
  #define ALARM_I2S_LRCK      AUDIO_I2S_LRC
  #define ALARM_I2S_DOUT      AUDIO_I2S_DOUT
#else
  #undef  ALARM_AUDIO_MODE
  #define ALARM_AUDIO_MODE    0
#endif

#if !BOARD_HAS_DISPLAY || !BOARD_HAS_TOUCH
  #undef  ENABLE_UI
  #define ENABLE_UI 0
#endif
#if !ENABLE_UI || !ENABLE_BUTTONS
  #undef  ENABLE_UI_PWRMENU
  #define ENABLE_UI_PWRMENU 0
#endif
#if !ENABLE_WIFI
  #undef  ENABLE_WEB_PORTAL
  #define ENABLE_WEB_PORTAL 0
  #undef  ENABLE_ARTISAN_WS
  #define ENABLE_ARTISAN_WS 0
#endif
