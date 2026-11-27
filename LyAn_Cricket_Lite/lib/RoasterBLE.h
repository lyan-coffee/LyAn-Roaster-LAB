/*
 * lib/RoasterBLE.h - HiBean over Bluetooth (NimBLE) (tier 5)
 *
 * UUIDs, NimBLE server, Nordic UART characteristic (RX write / TX notify), 4 PID characteristics, device info (180A),
 * single-packet advertising with the full name, LAZY init (only when HiBean is chosen), BLE fully stopped when HiBean is off (frees ~40 KB internal RAM).
 *
 * Wiring:
 *   - HiBean commands go to dialectPush() (command-string queue in machines/<machine>/RoasterDialect.h).
 *   - Replies (READ, CHAN) go out through bleNotify(), registered with dialectSetReply() in bleBegin().
 *   - hibeanWanted / bleStackUp / wifiRadioUp / ramLog are in lib/RoasterRadio.h.
 *   - The three display states (off / waiting for HiBean / connected) come from radioHibeanText() and g_state.hibeanLinked.
 *   - Device name / version come from DEVICE_NAME / FW_VERSION in RoasterSetting.h.
 *   - notify() is called only while HiBean is connected; the 30 ms wait (so HiBean sees a write-to-notify delay) also applies only then.
 *
 * Interface:
 *   bleBegin()    once in setup(): connects the Dialect reply to BLE. Does NOT init NimBLE.
 *   bleService()  every loop() (core 1): starts / stops BLE per hibeanWanted. NimBLE is touched from core 1 only.
 *   HiBean on: radioWantHibean(true)   off: radioWantHibean(false)   (lib/RoasterRadio.h)
 *
 * BLE and WiFi share the antenna and internal RAM, so they NEVER run together: bleService() initializes only when wifiRadioUp = false,
 * and RoasterNet does not enable WiFi while bleStackUp = true. So with HiBean on, Web / OTA over WiFi is unavailable.
 *
 * ENABLE_HIBEAN_BLE = 0: empty functions only, NimBLE library not used.
 * Depends on: Setting, Sync, State, PID (myPID, myPIDConfig), Radio, machines/RoasterMachines.h (Dialect).
 * ----------------------------------------------------------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"
#include "RoasterState.h"
#include "RoasterPID.h"
#include "RoasterRadio.h"
#include "../machines/RoasterMachines.h"

#if ENABLE_HIBEAN_BLE
#define HAS_MODULE_BLE 1
#include <NimBLEDevice.h>
#include <string>

// -----------------------------------------------------------------------------
// UUIDs
// -----------------------------------------------------------------------------
#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e" // NUS service
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e" // Write
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e" // Notify

#define PID_TUNE          "6dbf0201-758d-4b5e-bc11-40cfaea42dfe" // ppp.pp,iii.ii,ddd.dd
#define PID_MODE          "6dbf0202-758d-4b5e-bc11-40cfaea42dfe" // "P_ON_M" | "P_ON_E"
#define PID_SAMPLE_TIME   "6dbf0203-758d-4b5e-bc11-40cfaea42dfe" // iiii (ms)
#define PID_MAX_POWER     "6dbf0204-758d-4b5e-bc11-40cfaea42dfe" // 0-100 (%)

// -----------------------------------------------------------------------------
// Module variables
// -----------------------------------------------------------------------------
static NimBLEServer *pServer = nullptr;
static NimBLECharacteristic *pTxCharacteristic = nullptr;

static volatile bool deviceConnected = false;        // HiBean connected
static bool          bleActive = false;              // actual advertising state
static uint16_t      g_connHandle = 0;

// -----------------------------------------------------------------------------
// Server callbacks
// -----------------------------------------------------------------------------
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override {
    deviceConnected = true;
    g_state.hibeanLinked = true;
    g_connHandle = connInfo.getConnHandle();
    g_lastHiBeanMs = millis();
    // min 15 ms, max 30 ms, latency 4, timeout 5 s (Apple recommendation)
    pServer->updateConnParams(connInfo.getConnHandle(), 12, 24, 4, 500);
    D_println("BLE: Client connected.");
  }
  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &, int) override {
    deviceConnected = false;
    g_state.hibeanLinked = false;
    D_println("BLE: Client disconnected.");
    if (hibeanWanted) pServer->getAdvertising()->start();
  }
};

// -----------------------------------------------------------------------------
// Command from HiBean -> Dialect queue (processed by loop)
// -----------------------------------------------------------------------------
class RoasterCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &) override {
    String rxValue = String(pCharacteristic->getValue().c_str());
    rxValue.trim();
    if (rxValue.length() > 0) {
      DLOG("[HiBean ->] %s\n", rxValue.c_str());          // log convention: "->" = command sent to the roaster; "<-" = read from the roaster (printed in RoasterDialect.h)
      dialectPush(rxValue);                                // max DIALECT_QUEUE_MAX (32) commands, extras dropped
    }
  }
};

// -----------------------------------------------------------------------------
// PID config characteristics
// -----------------------------------------------------------------------------
class PIDTuneCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &) override {
    String rxValue = String(pCharacteristic->getValue().c_str());
    double v[3] = { myPIDConfig.getKp(), myPIDConfig.getKi(), myPIDConfig.getKd() };
    int n = 0;
    while (rxValue.length() > 0 && n < 3) {
        int index = rxValue.indexOf(',');
        if (index == -1) { v[n++] = rxValue.toDouble(); break; }
        v[n++] = rxValue.substring(0, index).toDouble();
        rxValue = rxValue.substring(index + 1);
    }
    LOCK();
    myPIDConfig.setKp(v[0]); myPIDConfig.setKi(v[1]); myPIDConfig.setKd(v[2]);
    myPIDConfig.apply(myPID);
    UNLOCK();
  }
  void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &) override {
      pCharacteristic->setValue(String(myPIDConfig.getKp()) + ',' + String(myPIDConfig.getKi()) + ',' + String(myPIDConfig.getKd()));
  }
};

class PIDModeCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &) override {
    String rxValue = String(pCharacteristic->getValue().c_str());
    LOCK();
    myPIDConfig.setPMode(rxValue == "P_ON_E" ? P_ON_E : P_ON_M);
    myPIDConfig.apply(myPID);
    UNLOCK();
  }
  void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &) override {
      pCharacteristic->setValue(myPIDConfig.getPMode() == P_ON_E ? "P_ON_E" : "P_ON_M");
  }
};

class PIDSampleTimeCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &) override {
    String rxValue = String(pCharacteristic->getValue().c_str());
    LOCK();
    myPIDConfig.setSampleTime(rxValue.toInt());
    myPIDConfig.apply(myPID);
    UNLOCK();
  }
  void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &) override {
      pCharacteristic->setValue(String(myPIDConfig.getSampleTime()));
  }
};

class PIDMaxPowerCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &) override {
    String rxValue = String(pCharacteristic->getValue().c_str());
    LOCK();
    myPIDConfig.setMaxPower(rxValue.toInt());
    myPIDConfig.apply(myPID);
    UNLOCK();
  }
  void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &) override {
      pCharacteristic->setValue(String(myPIDConfig.getMaxPower()));
  }
};

// -----------------------------------------------------------------------------
// Notify a reply to HiBean (Dialect calls it from loop, core 1)
// -----------------------------------------------------------------------------
static void bleNotify(const String &message) {
    if (!deviceConnected || !pTxCharacteristic) return;
    delay(30); // so HiBean sees a delay between write and notify
    if (deviceConnected && pTxCharacteristic) {
        pTxCharacteristic->setValue(message.c_str());
        pTxCharacteristic->notify();
    }
}

// -----------------------------------------------------------------------------
// BLE init (sketchName / firmWareVersion replaced by DEVICE_NAME / FW_VERSION)
// -----------------------------------------------------------------------------
static void initBLE() {
    std::string NimBLEDeviceName = BLE_NAME;                 // RoasterSetting.h
    NimBLEDevice::init(NimBLEDeviceName);

    pServer = NimBLEDevice::createServer();
    static MyServerCallbacks s_srvCb;
    pServer->setCallbacks(&s_srvCb, false);        // false = NimBLE does not delete it (static object, reusable after deinit)

    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
    );

    NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    static RoasterCallbacks s_rxCb;
    pRxCharacteristic->setCallbacks(&s_rxCb);

    // PID_TUNE
    NimBLECharacteristic *pidTuneCharacteristic = pService->createCharacteristic(
        PID_TUNE, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    static PIDTuneCallback s_pidTuneCb;
    pidTuneCharacteristic->setCallbacks(&s_pidTuneCb);
    BLEDescriptor *pidTuneDescriptor = pidTuneCharacteristic->createDescriptor(PID_TUNE, NIMBLE_PROPERTY::READ);
    pidTuneDescriptor->setValue("PID Tune: p.pp,i.ii,d.dd");
    pidTuneCharacteristic->addDescriptor(pidTuneDescriptor);

    // PID_MODE
    NimBLECharacteristic *pidModeCharacteristic = pService->createCharacteristic(
        PID_MODE, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    static PIDModeCallback s_pidModeCb;
    pidModeCharacteristic->setCallbacks(&s_pidModeCb);
    BLEDescriptor *pidModeDescriptor = pidModeCharacteristic->createDescriptor(PID_MODE, NIMBLE_PROPERTY::READ);
    pidModeDescriptor->setValue("PID Mode: P_ON_M | P_ON_E");
    pidModeCharacteristic->addDescriptor(pidModeDescriptor);

    // PID_SAMPLE_TIME
    NimBLECharacteristic *pidSampletimeCharacteristic = pService->createCharacteristic(
        PID_SAMPLE_TIME, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    static PIDSampleTimeCallback s_pidSampCb;
    pidSampletimeCharacteristic->setCallbacks(&s_pidSampCb);
    BLEDescriptor *pidSampleTimeDescriptor = pidSampletimeCharacteristic->createDescriptor(PID_SAMPLE_TIME, NIMBLE_PROPERTY::READ);
    pidSampleTimeDescriptor->setValue("PID Sample Time: iiii (ms)");
    pidSampletimeCharacteristic->addDescriptor(pidSampleTimeDescriptor);

    // PID_MAX_POWER
    NimBLECharacteristic *pidMaxPowerCharacteristic = pService->createCharacteristic(
        PID_MAX_POWER, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    static PIDMaxPowerCallback s_pidPwrCb;
    pidMaxPowerCharacteristic->setCallbacks(&s_pidPwrCb);
    NimBLEDescriptor *pidMaxPowerDescriptor = pidMaxPowerCharacteristic->createDescriptor(PID_MAX_POWER, NIMBLE_PROPERTY::READ);
    pidMaxPowerDescriptor->setValue("PID Max Power: 0-100 (%)");
    pidMaxPowerCharacteristic->addDescriptor(pidMaxPowerDescriptor);

    pService->start();

    // Device info for HiBean
    NimBLEService *devInfoService = pServer->createService("180A");
    NimBLECharacteristic *boardCharacteristic = devInfoService->createCharacteristic("2A29", NIMBLE_PROPERTY::READ);
      boardCharacteristic->setValue(ARDUINO_BOARD);
    NimBLECharacteristic *sketchNameCharacteristic = devInfoService->createCharacteristic("2A28", NIMBLE_PROPERTY::READ);
      sketchNameCharacteristic->setValue(DEVICE_NAME);
    NimBLECharacteristic *firmwareCharacteristic = devInfoService->createCharacteristic("2A26", NIMBLE_PROPERTY::READ);
      firmwareCharacteristic->setValue(String(DEVICE_NAME) + " " + FW_VERSION);
    devInfoService->start();

    // Advertise ONE packet only (no Scan Response) = flags (3 bytes) + FULL name (max 26 chars). Do not split a short name into another packet:
    // the HiBean app / Windows treat the main packet and the Scan Response as two devices and show 2 names.
    // A 27-char name in the main packet exceeds 31 bytes and is ignored, leaving the device unnamed.
    NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->enableScanResponse(false);
    {
        NimBLEAdvertisementData advData;
        advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
        advData.setName(NimBLEDeviceName.substr(0, 26), NimBLEDeviceName.size() <= 26);
        if (!pAdvertising->setAdvertisementData(advData)) { D_println("BLE: setAdvertisementData loi"); }
    }
    if (hibeanWanted) pAdvertising->start();
    bleActive = hibeanWanted;
    bleStackUp = true;
    ramLog("sau initBLE");
}

// -----------------------------------------------------------------------------
// Fully stop BLE: disconnect, stop advertising, shut down the NimBLE stack -> frees ~40 KB internal RAM and turns off 2.4 GHz.
// Needed because WiFi and BLE share one antenna: only ONE radio runs at a time.
// -----------------------------------------------------------------------------
static void bleDeinit() {
    if (!pServer) return;
    if (deviceConnected) pServer->disconnect(g_connHandle);
    NimBLEDevice::deinit(true);                   // true = xoa het server / service / characteristic
    pServer = nullptr;                            // initBLE() rebuilds it from scratch when HiBean is turned on
    pTxCharacteristic = nullptr;
    deviceConnected = false;
    g_state.hibeanLinked = false;
    bleActive = false;
    bleStackUp = false;
    ramLog("sau bleDeinit");
}

// -----------------------------------------------------------------------------
// Apply HiBean on/off (called from loop, core 1 - keeps NimBLE away from the UI thread)
// -----------------------------------------------------------------------------
static void bleService() {
    // LAZY NimBLE init: only when the user selects HiBean. NimBLE takes ~40 KB of internal RAM;
    // Both HiBean / Artisan buttons are available at boot, so NimBLE is not kept ready: leave the RAM to WiFi.
    if (hibeanWanted == bleActive) return;
    if (hibeanWanted) {
        if (wifiRadioUp && !radioCoexistNow()) return;   // WiFi still up: wait for it to stop before enabling BLE (they run together only with RADIO_COEXIST and auto mode)
        if (!pServer) initBLE();
        if (!pServer) return;                     // not enough RAM to init: leave as is, retry next time
        pServer->getAdvertising()->start();
        bleActive = true;
    } else {
        bleDeinit();                              // turning HiBean OFF = turning BLE fully off (not just stopping advertising)
    }
}

// Once in setup(): connect the Dialect reply to BLE. Does not init NimBLE.
static void bleBegin() {
    dialectSetReply(bleNotify);
}

#else   // ------------------------------------------------- ENABLE_HIBEAN_BLE 0
static inline void bleService() {}
static inline void bleBegin() {}
#endif  // ENABLE_HIBEAN_BLE
