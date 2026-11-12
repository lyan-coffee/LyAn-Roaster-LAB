/*
 * GPL v3
 * Lõi điều khiển máy rang: Heater, Air, Drum, Cool, PID, RoR và các luật an toàn.
 * HiBean và Artisan đều điều khiển qua các hàm trong file này.
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterState.h"
#include "RoasterPID.h"

enum Src : uint8_t { SRC_ARTISAN = 0, SRC_HIBEAN = 1 };
static Src heatSrc = SRC_ARTISAN;

// RoR (°C/phút): so BT hiện tại với ROR_WINDOW_S giây trước
static float    rorBuf[ROR_WINDOW_S + 1];
static int      rorHead = 0, rorCnt = 0;
static float    rorValue = 0.0f;
static bool     rorOk = false;
static uint32_t rorLastMs = 0;

static void rorUpdate() {
    const uint32_t now = millis();
    if (now - rorLastMs < 1000) return;
    rorLastMs = now;
    LOCK();
    if (!tempSignalOk()) {
        rorCnt = 0; rorHead = 0; rorOk = false;
    } else {
        rorBuf[rorHead] = (float)temp;
        rorHead = (rorHead + 1) % (ROR_WINDOW_S + 1);
        if (rorCnt < ROR_WINDOW_S + 1) rorCnt++;
        if (rorCnt == ROR_WINDOW_S + 1) {
            rorValue = ((float)temp - rorBuf[rorHead]) * 60.0f / (float)ROR_WINDOW_S;
            rorOk = true;
        } else {
            rorOk = false;
        }
    }
    UNLOCK();
}

// tiện ích
static void setValue(uint8_t *bytePtr, uint8_t value) {
    LOCK();
    *bytePtr = value;
    UNLOCK();
}

// Heater tay luôn bật được (trừ khi HEAT_REQUIRES_TEMP_SIGNAL = 1 và chưa có nhiệt độ)
static bool heatBlocked() { return HEAT_REQUIRES_TEMP_SIGNAL && tempSignalLost(); }
// PID cần có nhiệt độ mới bật được
static bool pidBlocked()  { return tempSignalLost(); }

void handleVENT(int value);
// Có Heater thì Air không thấp hơn MIN_AIR_FOR_HEAT
static void airForHeat() {
    if (sendBuffer[VENT_BYTE] < MIN_AIR_FOR_HEAT) handleVENT(MIN_AIR_FOR_HEAT);
}

// Cắt Heater + thoát PID
static void cutHeat() {
    LOCK();
    if (myPID.GetMode() == AUTOMATIC) myPID.SetMode(MANUAL);
    manualHeatLevel = 0;
    setValue(&sendBuffer[HEAT_BYTE], 0);
    UNLOCK();
}

// Kiểm tra an toàn mỗi vòng loop()
static void safetyWatchdog() {
    LOCK();
    bool heating = sendBuffer[HEAT_BYTE] > 0 || myPID.GetMode() == AUTOMATIC;
    if (OVERHEAT_CUT_C > 0 && tempSignalOk() && temp >= (double)OVERHEAT_CUT_C && heating) {
        cutHeat();
        heating = false;
        DLOG("[SAFETY] Qua nhiet -> cat Heater\n");
    }
    if (temp >= (double)DRUM_AUTO_ON_C && sendBuffer[DRUM_BYTE] == 0) setValue(&sendBuffer[DRUM_BYTE], 100);
    if (heating && (HEAT_REQUIRES_TEMP_SIGNAL ? tempSignalLost() : tempSignalDropped())) {
        cutHeat();
        heating = false;
        DLOG("[SAFETY] Mat tin hieu nhiet do -> cat Heater, tat PID\n");
    }
    if (heating && heatSrc == SRC_HIBEAN && myPID.GetMode() == AUTOMATIC && (millis() - g_lastHiBeanMs) > HIBEAN_TIMEOUT_MS)
        pidOffKeepHeat();                          // không ai giám sát PID: chuyển sang tay, giữ Heater
    UNLOCK();
}

// Xử lý lệnh (HiBean gọi thẳng, Artisan gọi qua ctl*)
void handleHEAT(int value) {
    if (value >= 0 && value <= 100) setValue(&sendBuffer[HEAT_BYTE], (uint8_t)value);
}

// Filter theo Air: 1-25 -> 4, 26-50 -> 3, 51-75 -> 2, 76-100 -> 1
static void syncFilterFromVent() {
    int v = sendBuffer[VENT_BYTE];
    int f = (v == 0) ? 0 : (4 - ((v - 1) * 4 / 100));
    setValue(&sendBuffer[FILTER_BYTE], (uint8_t)f);
}

void handleOT1(int value, Src src = SRC_HIBEAN) {
    LOCK();
    if (myPID.GetMode() == MANUAL) {
        int v = constrain(value, 0, 100);
        if (v > 0 && heatBlocked()) v = 0;
        manualHeatLevel = v;
        heatSrc = src;
        handleHEAT(v);
        if (v > 0) airForHeat();                       // Heater bật -> Air tự lên mức tối thiểu
    } else {
        // PID đang chạy: OT1 = công suất tối đa
        myPIDConfig.setMaxPower(constrain(value, 0, 100));
        myPIDConfig.apply(myPID);
    }
    UNLOCK();
}

void handleVENT(int value) {
    if (value < 0 || value > 100) return;
    LOCK();
    if (value < MIN_AIR_FOR_HEAT && (sendBuffer[HEAT_BYTE] > 0 || myPID.GetMode() == AUTOMATIC)) value = MIN_AIR_FOR_HEAT;   // còn Heater thì không hạ Air dưới mức tối thiểu
    setValue(&sendBuffer[VENT_BYTE], (uint8_t)value);
    syncFilterFromVent();
    UNLOCK();
}

void handleFILTER(int value) {
    if (value >= 0 && value <= 4) setValue(&sendBuffer[FILTER_BYTE], (uint8_t)value);
}

// BT >= DRUM_AUTO_ON_C: không cho tắt Drum
void handleDRUM(int value) {
    LOCK();
    if (value == 0 && temp >= (double)DRUM_AUTO_ON_C) value = 1;
    setValue(&sendBuffer[DRUM_BYTE], value != 0 ? 100 : 0);
    UNLOCK();
}

void handleCOOL(int value) {
    if (value < 0 || value > 100) return;
    LOCK();
    setValue(&sendBuffer[COOL_BYTE], (uint8_t)value);
    syncFilterFromVent();
    UNLOCK();
}

void eStop() {
    LOCK();
    cutHeat();
    handleVENT(100);
    UNLOCK();
}

// Tắt hết, riêng Drum vẫn chạy nếu BT còn cao
void roasterShutdown() {
    LOCK();
    for (int i = 0; i < CONTROLLER_LENGTH; i++) sendBuffer[i] = 0;
    if (temp >= (double)DRUM_AUTO_ON_C) sendBuffer[DRUM_BYTE] = 100;
    if (myPID.GetMode() == AUTOMATIC) myPID.SetMode(MANUAL);
    manualHeatLevel = 0;
    UNLOCK();
}

// Bật / tắt PID
void setPIDMode(bool usePID) {
    LOCK();
    if (usePID) {
        heatSrc = SRC_HIBEAN;
        if (!pidBlocked()) {
            pInput  = temp;                          // PID bắt đầu từ mức Heater hiện tại
            pOutput = sendBuffer[HEAT_BYTE];
            myPID.SetMode(AUTOMATIC);
            airForHeat();                            // PID điều khiển Heater -> Air tối thiểu
        }
    } else {
        pidOffKeepHeat();                            // tắt PID: giữ nguyên mức Heater hiện tại
    }
    UNLOCK();
}

// Chạy PID mỗi vòng loop()
void handlePIDControl() {
    LOCK();
    if (myPID.GetMode() == AUTOMATIC) {
        pInput = temp;
        if (myPIDConfig.getPMode() == P_ON_E && pSetpoint - temp > PID_I_BAND_C) {
            // Còn xa Set: nạp sẵn tích phân để không vọt qua Set
            pOutput = PID_I_PRESET;
            myPID.SetMode(MANUAL);
            myPID.SetMode(AUTOMATIC);
        }
        myPID.Compute();
        double hd = pOutput;
        {   // Hỗ trợ: xa Set thì ép Heater cao, vượt Set thì giảm / cắt Heater
            const double lead = (rorOk ? (double)rorValue : 0.0) * PID_LEAD_S / 60.0;   // BT dự kiến
            const double gap  = pSetpoint - (temp + (lead > 0 ? lead : 0.0));           // khoảng cách tới Set
            static bool boost = false;                                                   // "đang leo": bật khi cách Set > FAR, tắt khi sát Set
            if (gap > PID_BOOST_FAR_C) boost = true; else if (gap <= PID_BOOST_END_C) boost = false;
            if (boost && gap > PID_BOOST_FAR_C)       { if (hd < PID_BOOST_FAR_PCT)  hd = PID_BOOST_FAR_PCT; }
            else if (boost && gap > PID_BOOST_NEAR_C) { if (hd < PID_BOOST_NEAR_PCT) hd = PID_BOOST_NEAR_PCT; }
            const double over = temp - pSetpoint;                                        // độ C vượt Set
            if (over > PID_OVER_MAX_C)       hd = 0;
            else if (over > PID_OVER_SOFT_C) { if (hd > PID_OVER_SOFT_PCT) hd = PID_OVER_SOFT_PCT; }
        }
        int h = (int)(round(hd / 5.0) * 5);
        handleHEAT(constrain(h, 0, 100));
    }
    UNLOCK();
}

// Lệnh từ Artisan
void ctlSetHeater(int v) {
    v = constrain(v, 0, 100);
    v = ((v + 2) / 5) * 5;
    LOCK();
    if (myPID.GetMode() == AUTOMATIC) pidOffKeepHeat();     // chỉnh tay: thoát PID
    handleOT1(v, SRC_ARTISAN);
    UNLOCK();
}
void ctlSetPID(bool on) {
    LOCK();
    if (on) {
        if (!pidBlocked()) {
            if (pSetpoint <= 0) pSetpoint = DEFAULT_PID_SETPOINT;
            heatSrc = SRC_ARTISAN;
            pInput  = temp;                                  // PID bắt đầu từ mức Heater hiện tại
            pOutput = sendBuffer[HEAT_BYTE];
            myPID.SetMode(AUTOMATIC);
            airForHeat();
        }
    } else {
        pidOffKeepHeat();
    }
    UNLOCK();
}
void ctlSetSetpoint(float c) {
    LOCK();
    pSetpoint = constrain((double)(round(c / 5.0f) * 5), 0.0, 300.0);   // bước 5 độ C
    UNLOCK();
}
void ctlSetAir(int v)     { handleVENT(constrain(v, 0, 100)); }
void ctlSetDrum(bool on)  { handleDRUM(on ? 100 : 0); }
void ctlSetCool(bool on)  { handleCOOL(on ? 100 : 0); }

// Trạng thái hiện tại (cho Artisan)
struct Snap {
    int   heat, vent;
    bool  drum, cool, pid, tempOk;
    float temp, sp;
};

static void roasterSnapshot(Snap &s) {
    LOCK();
    s.heat   = sendBuffer[HEAT_BYTE];
    s.vent   = sendBuffer[VENT_BYTE];
    s.drum   = sendBuffer[DRUM_BYTE] != 0;
    s.cool   = sendBuffer[COOL_BYTE] != 0;
    s.pid    = (myPID.GetMode() == AUTOMATIC);
    s.temp   = (float)temp;
    s.sp     = (float)pSetpoint;
    s.tempOk = tempSignalOk();
    UNLOCK();
}

// Đang cấp nhiệt: không cho nạp firmware
static bool controlBusy() {
    LOCK();
    const bool b = sendBuffer[HEAT_BYTE] > 0 || myPID.GetMode() == AUTOMATIC;
    UNLOCK();
    return b;
}

// Gọi trong setup()
static void controlInit() {
    pidInit();
    roasterShutdown();                // Heater 0%, mọi thứ tắt
}
