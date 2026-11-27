/*
 * lib/RoasterPID.h - Temperature PID controller (tier 2)
 *
 * Contents: PIDConfig (Kp, Ki, Kd, P mode, sample time, max power; apply() pushes it into PID_v1),
 * pInput / pOutput / pSetpoint / manualHeatLevel, myPIDConfig, myPID, pidOffKeepHeat().
 * Heater compensation (boost, brake, overshoot guard) is in handlePIDControl() in RoasterControl.h.
 * Requires: PID (br3ttb) 1.2.0.
 * ----------------------------------------------------------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include <PID_v1.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"
#include "RoasterState.h"

#define HAS_MODULE_PID 1

class PIDConfig
{
public:
    PIDConfig()
        : kP_(PID_DEFAULT_KP),
          kI_(PID_DEFAULT_KI),
          kD_(PID_DEFAULT_KD),
          sampleTime_(PID_SAMPLE_MS),
          pMode_(PID_DEFAULT_PMODE ? P_ON_E : P_ON_M),
          maxPower_(100)
    {
    }

    // --- Getters ---
    double getKp() const { return kP_; }
    double getKi() const { return kI_; }
    double getKd() const { return kD_; }
    int   getSampleTime() const { return sampleTime_; }
    int    getPMode() const { return pMode_; }
    int    getMaxPower() const { return maxPower_; }

    // --- Setters ---
    void setKp(double kp) { kP_ = kp; }
    void setKi(double ki) { kI_ = ki; }
    void setKd(double kd) { kD_ = kd; }

    void setSampleTime(int sampleTime)
    {
        if (sampleTime > 0)
            sampleTime_ = sampleTime;
    }

    void setPMode(int mode)
    {
        if (mode == P_ON_E || mode == P_ON_M)
            pMode_ = mode;
    }

    void setMaxPower(int maxPower)
    {
        if (maxPower >= 0 && maxPower <= 100)
            maxPower_ = maxPower;
    }

    // --- Apply to Arduino PID ---
    void apply(PID& pid) const
    {
        pid.SetTunings(kP_, kI_, kD_, pMode_);
        pid.SetSampleTime(sampleTime_);
        pid.SetOutputLimits(0, maxPower_);
    }

private:
    double kP_;
    double kI_;
    double kD_;
    int   sampleTime_;
    int    pMode_;
    int    maxPower_;
};

// ---- PID core ----
static double pInput = 0.0, pOutput = 0.0;
static double pSetpoint = DEFAULT_PID_SETPOINT;   // °C
static int    manualHeatLevel = 0;

static PIDConfig myPIDConfig;
static PID myPID(&pInput, &pOutput, &pSetpoint,
                 myPIDConfig.getKp(), myPIDConfig.getKi(), myPIDConfig.getKd(),
                 myPIDConfig.getPMode(), DIRECT);

// Call once in setup(): PID in manual mode, limits and sample time from config
static void pidInit() {
    myPID.SetMode(MANUAL);
    myPID.SetOutputLimits(0.0, myPIDConfig.getMaxPower());
    myPID.SetSampleTime(myPIDConfig.getSampleTime());
}

// Turn PID off but KEEP the current heater level (switch to manual, do not drop to 0)
static void pidOffKeepHeat() {
    LOCK();
    if (myPID.GetMode() == AUTOMATIC) {
        myPID.SetMode(MANUAL);
        manualHeatLevel = sendBuffer[HEAT_BYTE];
    }
    UNLOCK();
}
