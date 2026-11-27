/*
 * boards/RoasterBoard.h - BOARD SELECTOR
 *
 * Included at the bottom of RoasterSetting.h. Picks exactly ONE board file by the BOARD_... flag of RoasterSetting.h (section 2).
 * To add a board: copy the existing board file, edit the numbers, add one flag and one #elif here.
 * Modules never know the board name: they only read BOARD_*, SCREEN_* and pin macros.
 *
 * Every board file MUST define: BOARD_NAME, BOARD_CHIP, BOARD_FLASH_MB, BOARD_PSRAM_MB,
 *   BOARD_HAS_BLE / WIFI / PSRAM / DISPLAY / TOUCH / BUTTONS / SD / BATTERY / AUDIO,
 *   BOARD_SCREEN_INCH_X100, SCREEN_W, SCREEN_H,
 *   BOARD_FLASH_HINT, BOARD_PSRAM_HINT, BOARD_PARTITION_HINT, BOARD_OTA_MIN_KB.
 */
#pragma once

#if (BOARD_WAVESHARE_S3_TOUCH_LCD_154 + 0) == 1
  #include "Board_Waveshare_ESP32S3_Touch_LCD_154.h"
#else
  #error "No board selected: set exactly ONE BOARD_... flag to 1 in section 2 of RoasterSetting.h"
#endif

#ifndef BOARD_NAME
  #error "Board file lacks BOARD_NAME"
#endif
#ifndef BOARD_HAS_DISPLAY
  #error "Board file lacks BOARD_HAS_* capability macros"
#endif
#ifndef SCREEN_W
  #error "Board file lacks SCREEN_W / SCREEN_H"
#endif
#ifndef BOARD_PARTITION_HINT
  #error "Board file lacks BOARD_*_HINT / BOARD_OTA_MIN_KB"
#endif
