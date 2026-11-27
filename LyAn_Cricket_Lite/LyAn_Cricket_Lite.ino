/*
 * LyAn_Cricket_Lite.ino - Open-Source Community Edition (GPLv3)
 *
 * Board   : Waveshare ESP32-S3-Touch-LCD-1.54 (ST7789 240x240, CST816S, BOOT/PWR/PLUS, I2S audio, battery)
 * Roaster : Skywalker V1 (Manual control, standard PID, HiBean BLE, Artisan WebSocket, Web Portal)
 * UI      : LVGL 9.2.x (EN / VI / CN, Welcome + cricket cue, IDLE logo, Home / Control / Event / Settings)
 *
 * Copyright (C) 2026 LyAn Roaster LAB (lycoffee.ai@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <Arduino.h>
#include <esp_system.h>

#include "RoasterSetting.h"
#include "lib/UIConfig.h"
#include "lib/RoasterSync.h"
#include "lib/RoasterState.h"
#include "lib/RoasterClock.h"
#include "lib/RoasterBatch.h"
#include "lib/RoasterPID.h"
#include "lib/RoasterControl.h"
#include "machines/RoasterMachines.h"

#include "lib/RoasterRadio.h"
#include "lib/RoasterTick.h"
#include "lib/RoasterSim.h"
#if ENABLE_ROAST_GRAPH
  #include "lib/RoasterGraphData.h"
#endif
#include "lib/RoasterSettings.h"
#include "lib/RoasterNet.h"
#include "lib/RoasterWeb.h"
#include "lib/RoasterBLE.h"
#include "lib/RoasterArtisan.h"
#include "lib/RoasterWebArtisan.h"

#if ENABLE_BATTERY
  #include "lib/RoasterBattery.h"
#endif
#if ENABLE_BUTTONS
  #include "lib/RoasterButtons.h"
#endif
#if ENABLE_DISPLAY
  #include "lib/RoasterDisplay.h"
  #if ENABLE_TOUCH
    #include "lib/RoasterTouch.h"
  #endif
  #include "lib/RoasterUIBind.h"
  #include "lib/RoasterUI.h"
#endif

// Long-press PLUS (>= 0.8s) while idle: cycle link mode (Standalone -> HiBean -> Artisan -> Web Portal)
#if ENABLE_BUTTONS
static void radioSwitchLongPress() {
    if (batchLive || roasting) return;
    panelHeaterOff();
    const uint8_t next = (radioLinkMode() + 1) % LINK_COUNT;
    netLinkMode(next, true);
}
#endif

void setup() {
#if SERIAL_DEBUG
    Serial.begin(SERIAL_BAUD);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < SERIAL_WAIT_MS) delay(10);
    DLOG("\n%s %s (%s)\n", DEVICE_NAME, FW_VERSION, FW_STAGE);
#endif
    syncInit();
#if ENABLE_BATTERY
    batteryHold();
#endif
    controlInit();
    settingsLoad();
    clockInit();

#if ENABLE_TOUCH
    touchBegin();
#endif
#if ENABLE_BUTTONS
    buttonsBegin();
    buttonSetLongHook(BTN_PLUS, radioSwitchLongPress);
#endif
#if ENABLE_BATTERY
    batteryBegin();
#endif

    // Network, Web Portal, HiBean BLE, and Artisan WebSocket
    netInit();
    webInit();
    bleBegin();
    artisanInit();
    netLinkBoot();
    webArtisanInit();

    tickAdd(netTick);
    tickAdd(webTick);
    tickAdd(artisanTick);
#if ENABLE_BATTERY
    tickAdd(batteryTick);
#endif
#if !ENABLE_DISPLAY
    tickStartTask();
#endif

#if ENABLE_DISPLAY
    uiBindCoreV3();
    startUI();
#endif
}

void loop() {
#if ENABLE_BUTTONS
    buttonsPoll();
#endif

    bleService();
    machinePoll();
    if (simUpdate()) machineSendOff();
    rorUpdate();
#if ENABLE_ROAST_GRAPH
    graphUpdate();
#endif
    controlDrainQueue();
    dialectDrain();
    safetyWatchdog();
    enforceInterlock();
    handlePIDControl();
    controlSyncState();

    const bool sent = machineSend();
    delay(sent ? 1 : 5);
}
