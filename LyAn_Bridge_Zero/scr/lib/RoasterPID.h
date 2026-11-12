/*
 * GPL v3
 * PID nhiệt độ (thư viện PID br3ttb). Phần hỗ trợ tăng nhiệt / chống vọt nằm ở RoasterControl.h.
 */
#pragma once
#include <Arduino.h>
#include <PID_v1.h>
#include "../RoasterSetting.h"
#include "RoasterState.h"

class PIDConfig {
public:
    double getKp() const { return kP_; }
    double getKi() const { return kI_; }
    double getKd() const { return kD_; }
    int    getSampleTime() const { return sampleTime_; }
    int    getPMode() const { return pMode_; }
    int    getMaxPower() const { return maxPower_; }

    void setKp(double v) { kP_ = v; }
    void setKi(double v) { kI_ = v; }
    void setKd(double v) { kD_ = v; }
    void setSampleTime(int v) { if (v > 0) sampleTime_ = v; }
    void setPMode(int m) { if (m == P_ON_E || m == P_ON_M) pMode_ = m; }
    void setMaxPower(int v) { if (v >= 0 && v <= 100) maxPower_ = v; }

    void apply(PID &pid) const {
        pid.SetTunings(kP_, kI_, kD_, pMode_);
        pid.SetSampleTime(sampleTime_);
        pid.SetOutputLimits(0, maxPower_);
    }

private:
    double kP_ = PID_DEFAULT_KP, kI_ = PID_DEFAULT_KI, kD_ = PID_DEFAULT_KD;
    int    sampleTime_ = 500;
    int    pMode_ = PID_DEFAULT_PMODE ? P_ON_E : P_ON_M;
    int    maxPower_ = 100;
};

static double pInput = 0.0, pOutput = 0.0;
static double pSetpoint = DEFAULT_PID_SETPOINT;   // °C
static int    manualHeatLevel = 0;

static PIDConfig myPIDConfig;
static PID myPID(&pInput, &pOutput, &pSetpoint,
                 myPIDConfig.getKp(), myPIDConfig.getKi(), myPIDConfig.getKd(),
                 myPIDConfig.getPMode(), DIRECT);

// Gọi trong setup(): PID ở chế độ tay
static void pidInit() {
    myPID.SetMode(MANUAL);
    myPID.SetOutputLimits(0.0, myPIDConfig.getMaxPower());
    myPID.SetSampleTime(myPIDConfig.getSampleTime());
}

// Tắt PID, giữ nguyên mức Heater
static void pidOffKeepHeat() {
    LOCK();
    if (myPID.GetMode() == AUTOMATIC) {
        myPID.SetMode(MANUAL);
        manualHeatLevel = sendBuffer[HEAT_BYTE];
    }
    UNLOCK();
}
