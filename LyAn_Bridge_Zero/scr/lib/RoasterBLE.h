/*
 * GPL v3
 * HiBean qua Bluetooth (NimBLE): kênh lệnh UART, 4 mục PID, thông tin thiết bị.
 */
#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string>
#include "../RoasterSetting.h"
#include "RoasterState.h"
#include "RoasterPID.h"
#include "RoasterRadio.h"
#include "RoasterDialect.h"

#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e" // dịch vụ UART
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e" // HiBean ghi lệnh
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e" // máy trả lời

#define PID_TUNE          "6dbf0201-758d-4b5e-bc11-40cfaea42dfe" // ppp.pp,iii.ii,ddd.dd
#define PID_MODE          "6dbf0202-758d-4b5e-bc11-40cfaea42dfe" // "P_ON_M" | "P_ON_E"
#define PID_SAMPLE_TIME   "6dbf0203-758d-4b5e-bc11-40cfaea42dfe" // iiii (ms)
#define PID_MAX_POWER     "6dbf0204-758d-4b5e-bc11-40cfaea42dfe" // 0-100 (%)

static NimBLEServer *pServer = nullptr;
static NimBLECharacteristic *pTxCharacteristic = nullptr;

static volatile bool deviceConnected = false;
static bool          bleActive = false;
static uint16_t      g_connHandle = 0;

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override {
    deviceConnected = true;
    g_hibeanLinked = true;
    g_connHandle = connInfo.getConnHandle();
    g_lastHiBeanMs = millis();
    // xin nhịp kết nối 15-30 ms (khuyến nghị cho iPhone)
    pServer->updateConnParams(connInfo.getConnHandle(), 12, 24, 4, 500);
    DLOG("BLE: HiBean connected\n");
  }
  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &, int) override {
    deviceConnected = false;
    g_hibeanLinked = false;
    DLOG("BLE: HiBean disconnected\n");
    if (hibeanWanted) pServer->getAdvertising()->start();
  }
};

// Lệnh HiBean gửi -> hàng đợi lệnh
class RoasterCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &) override {
    String rxValue = String(pCharacteristic->getValue().c_str());
    rxValue.trim();
    if (rxValue.length() > 0) {
      DLOG("[HiBean ->] %s\n", rxValue.c_str());
      dialectPush(rxValue);
    }
  }
};

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

// Gửi trả lời cho HiBean
static void bleNotify(const String &message) {
    if (!deviceConnected || !pTxCharacteristic) return;
    delay(30);
    if (deviceConnected && pTxCharacteristic) {
        pTxCharacteristic->setValue(message.c_str());
        pTxCharacteristic->notify();
    }
}

// Tạo một mục PID (đọc / ghi)
static NimBLECharacteristic *blePidChar(NimBLEService *svc, const char *uuid, NimBLECharacteristicCallbacks *cb, const char *desc) {
    NimBLECharacteristic *c = svc->createCharacteristic(uuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    c->setCallbacks(cb);
    NimBLEDescriptor *d = c->createDescriptor(uuid, NIMBLE_PROPERTY::READ);
    d->setValue(desc);
    c->addDescriptor(d);
    return c;
}

static void initBLE() {
    std::string NimBLEDeviceName = BLE_NAME;
    NimBLEDevice::init(NimBLEDeviceName);

    pServer = NimBLEDevice::createServer();
    static MyServerCallbacks s_srvCb;
    pServer->setCallbacks(&s_srvCb, false);

    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);

    NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    static RoasterCallbacks s_rxCb;
    pRxCharacteristic->setCallbacks(&s_rxCb);

    static PIDTuneCallback       s_pidTuneCb;
    static PIDModeCallback       s_pidModeCb;
    static PIDSampleTimeCallback s_pidSampCb;
    static PIDMaxPowerCallback   s_pidPwrCb;
    blePidChar(pService, PID_TUNE,        &s_pidTuneCb, "PID Tune: p.pp,i.ii,d.dd");
    blePidChar(pService, PID_MODE,        &s_pidModeCb, "PID Mode: P_ON_M | P_ON_E");
    blePidChar(pService, PID_SAMPLE_TIME, &s_pidSampCb, "PID Sample Time: iiii (ms)");
    blePidChar(pService, PID_MAX_POWER,   &s_pidPwrCb,  "PID Max Power: 0-100 (%)");

    pService->start();

    // Thông tin thiết bị (HiBean đọc phiên bản firmware)
    NimBLEService *devInfoService = pServer->createService("180A");
    NimBLECharacteristic *boardCharacteristic = devInfoService->createCharacteristic("2A29", NIMBLE_PROPERTY::READ);
      boardCharacteristic->setValue(ARDUINO_BOARD);
    NimBLECharacteristic *sketchNameCharacteristic = devInfoService->createCharacteristic("2A28", NIMBLE_PROPERTY::READ);
      sketchNameCharacteristic->setValue(DEVICE_NAME);
    NimBLECharacteristic *firmwareCharacteristic = devInfoService->createCharacteristic("2A26", NIMBLE_PROPERTY::READ);
      firmwareCharacteristic->setValue(String(DEVICE_NAME) + " " + FW_VERSION);
    devInfoService->start();

    // Quảng bá một gói có tên đầy đủ (để app không thấy hai tên)
    NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->enableScanResponse(false);
    {
        NimBLEAdvertisementData advData;
        advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
        advData.setName(NimBLEDeviceName.substr(0, 26), NimBLEDeviceName.size() <= 26);
        if (!pAdvertising->setAdvertisementData(advData)) { DLOG("BLE: setAdvertisementData loi\n"); }
    }
    if (hibeanWanted) pAdvertising->start();
    bleActive = hibeanWanted;
    bleStackUp = true;
}

// Tắt hẳn BLE để trả RAM cho WiFi
static void bleDeinit() {
    if (!pServer) return;
    if (deviceConnected) pServer->disconnect(g_connHandle);
    NimBLEDevice::deinit(true);
    pServer = nullptr;
    pTxCharacteristic = nullptr;
    deviceConnected = false;
    g_hibeanLinked = false;
    bleActive = false;
    bleStackUp = false;
}

// Bật / tắt BLE theo hibeanWanted (gọi từ loop)
static void bleService() {
    if (hibeanWanted == bleActive) return;
    if (hibeanWanted) {
        if (!pServer) initBLE();
        if (!pServer) return;                      // thiếu RAM: lần sau thử lại
        pServer->getAdvertising()->start();
        bleActive = true;
    } else {
        bleDeinit();
    }
}

static void bleBegin() { dialectSetReply(bleNotify); }
