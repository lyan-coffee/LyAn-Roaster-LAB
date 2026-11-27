/*
 * boards/Board_Waveshare_ESP32S3_Touch_LCD_154.h - BOARD DATA (tier 0: includes nothing)
 *
 * Waveshare ESP32-S3-Touch-LCD-1.54: ESP32-S3R8, flash 16 MB, PSRAM 8 MB, 1.54" 240x240 ST7789 (SPI), CST816 touch,
 * ES8311 codec + ES7210 mic ADC + NS4150B amp, TF slot, QMI8658 IMU, 3.7 V battery.
 *
 * Arduino IDE settings (Waveshare asks core 3.2.0; user has Espressif core 3.3.12 installed [not tested yet]):
 *   Board: ESP32S3 Dev Module | Flash Size: 16MB (128Mb) | PSRAM: OPI PSRAM [ASSUMPTION: S3R8 = octal PSRAM]
 *   USB CDC On Boot: Enabled | Partition: partitions.csv in the sketch folder (see its header)
 *
 * PIN SOURCES. Pins below come from the proposal of the other C0 chat (Project doc claude/bao-cao/C0.md), which cites
 * the Waveshare repo examples/ESP32-S3-Touch-LCD-1.54-demo (09_lvgl_arduino_v9.ino, 02_button_example.ino,
 * 07_sd_card_test.ino, bsp_*.h/.c, factory main.c). This chat could NOT open those files (GitHub blocked),
 * so every pin was [READ via other chat]. CHECKED ON THE BOARD [MEASURED, logs of C0 piece 2-3]: display DC 45 / CS 21 / SCK 38 /
 * MOSI 39 / RST 40, backlight 46, touch I2C SDA 42 / SCL 41 + address 0x15 + RST 47, buttons 0 / 5 / 4, TF card CLK 16 / CMD 15 / D0 17 / D1 18 / D2 13 / D3 14 (piece 4 log: mounted). NOT yet checked: battery pins.
 * The Waveshare Arduino page also shows another display pin set (DC 3, CS 5, SCK 1, MOSI 2, RST 4, BL 6): NOT used here,
 * because 5 and 4 are button pins in the same page.
 */
#pragma once

// ---- Identity ----
#define BOARD_NAME            "Waveshare ESP32-S3-Touch-LCD-1.54"
#define BOARD_CHIP            "ESP32-S3R8"
#define BOARD_FLASH_MB        16
#define BOARD_PSRAM_MB        8

// ---- Capabilities ----
#define BOARD_HAS_BLE         1
#define BOARD_HAS_WIFI        1
#define BOARD_HAS_PSRAM       1
#define BOARD_HAS_DISPLAY     1
#define BOARD_HAS_TOUCH       1
#define BOARD_HAS_BUTTONS     1
#define BOARD_HAS_SD          1
#define BOARD_HAS_BATTERY     1
#define BOARD_HAS_AUDIO       1

// ---- Hints printed in the boot report when a setting is wrong ----
#define BOARD_FLASH_HINT      "set Tools > Flash Size = 16MB (128Mb)"
#define BOARD_PSRAM_HINT      "set Tools > PSRAM = OPI PSRAM"
#define BOARD_PARTITION_HINT  "keep partitions.csv in the sketch folder (2 x 3.5 MB app + model + ffat)"
#define BOARD_OTA_MIN_KB      3072      // the second app slot must be at least this big (KB) to report OK

// ---- Display (ST7789, 4-wire SPI, no MISO) ----
#define BOARD_SCREEN_INCH_X100 154
#define BOARD_SCREEN_INCH_X10  15       // 1.54 inch (x100 so no float is needed in #if)
#define SCREEN_W              240
#define SCREEN_H              240
#define TFT_DC                45
#define TFT_CS                21
#define TFT_SCK               38
#define TFT_MOSI              39
#define TFT_MISO              -1
#define TFT_RST               40
#define TFT_BLK               46        // backlight

// ---- Touch CST816 (I2C bus shared with codec) ----
#define TOUCH_SDA             42
#define TOUCH_SCL             41
#define TOUCH_INT             48
#define TOUCH_RST             47
#define TOUCH_ADDR            0x15      // [MEASURED: I2C scan found it, piece 3]
#define TOUCH_I2C_HZ          400000
#define TOUCH_SWAP_XY         0         // adjust after touching the 4 corners (log shows x / y): swap axes ...
#define TOUCH_INVERT_X        0         // ... and/or mirror them
#define TOUCH_INVERT_Y        0

// ---- Buttons (active LOW) ----
#define BTN_BOOT_PIN          0
#define BTN_PWR_PIN           5
#define BTN_PLUS_PIN          4

// ---- TF card (SD_MMC, 4-bit) ----
#define SD_CLK_PIN            16
#define SD_CMD_PIN            15
#define SD_D0_PIN             17
#define SD_D1_PIN             18
#define SD_D2_PIN             13
#define SD_D3_PIN             14

// ---- Battery ----
#define BAT_ADC_PIN           1         // ADC1_CH0
#define BAT_ADC_MULT          3         // [ASSUMPTION] measured volts = ADC volts x 3 (divider); check against a multimeter
#define BAT_HOLD_PIN          2         // [ASSUMPTION] drive HIGH at boot so the board stays on when running from battery
#define BAT_CHG_PIN           3         // charge status input; which level means "charging": [UNKNOWN]

// ---- Audio pins: RESERVED for C3, not used by C0 (Waveshare 01_audio_out / 10_esp_sr) ----
#define AUDIO_PA_CTRL_PIN     7
#define AUDIO_I2S_MCLK        8
#define AUDIO_I2S_BCLK        9
#define AUDIO_I2S_LRC         10
#define AUDIO_I2S_DIN         11
#define AUDIO_I2S_DOUT        12

// ---- Skywalker V1 roaster wire (default GPIO 19/20 after ROASTER_BOOT_DELAY_MS, or override in RoasterSetting.h) ----
#ifndef ROASTER_TX_PIN
  #define ROASTER_TX_PIN      19
#endif
#ifndef ROASTER_RX_PIN
  #define ROASTER_RX_PIN      20
#endif
