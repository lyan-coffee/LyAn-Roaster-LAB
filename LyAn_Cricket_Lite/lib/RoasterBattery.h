/*
 * lib/CricketBattery.h - BATTERY VOLTAGE + CHARGE PIN + POWER HOLD (item F of C0)             (tier 1)
 *
 *   batteryHold()     call FIRST in setup(), silent: drives BAT_HOLD_PIN HIGH so the board stays on when running from battery
 *   batteryBegin()    ADC setup, first sample, one [BAT] log line
 *   batteryTick()     register with tickAdd(): samples every BATTERY_SAMPLE_MS, logs every BATTERY_LOG_MS, logs a charge-pin change at once
 *   batteryLog()      print one [BAT] line now (Serial command 'b')
 *   batteryPowerOff() cut the power (C2a power menu). Returns only when the board stays on (USB / hold pin not in control)
 *   batteryMv()       last battery voltage in mV (adc mV x BAT_ADC_MULT), thread safe
 *   batteryChargePin() last level of the charge-status pin (0 / 1), thread safe
 * ASSUMED (see boards/): divider x3, GPIO2 = power hold, charge-pin polarity UNKNOWN. This file only MEASURES and LOGS; there is no
 * percent and no charge decision yet. Compare the log with a multimeter, plug / unplug USB and watch the charge pin.
 * No String, no malloc. Values shared between cores go through LOCK().
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"

#define HAS_MODULE_BATTERY 1

static int  g_batAdcMv = 0;      // mV at the ADC pin (average)
static int  g_batNoiseMv = 0;    // max - min of the samples in one reading (rough noise figure)
static int  g_batMv = 0;         // mV of the battery = g_batAdcMv * BAT_ADC_MULT
static int  g_batChg = -1;       // level of BAT_CHG_PIN, -1 = not read yet
static bool g_batHold = false;

static void batteryHold() {
#if BAT_HOLD_ENABLE
    pinMode(BAT_HOLD_PIN, OUTPUT);
    digitalWrite(BAT_HOLD_PIN, HIGH);
    g_batHold = true;
#endif
}

// Power off by pulling BAT_HOLD_PIN LOW. On battery the board shuts down and this does not return.
// Returns false if still running (USB attached, or the hold pin does not control power); the pin is then set back HIGH.
static bool batteryPowerOff() {
#if BAT_HOLD_ENABLE
    DLOGM("BAT", "power off: hold pin GPIO%d -> LOW\n", BAT_HOLD_PIN);
    delay(30);                                           // let Serial flush the log line
    digitalWrite(BAT_HOLD_PIN, LOW);
    g_batHold = false;
    const uint32_t t0 = millis();
    while ((uint32_t)(millis() - t0) < (uint32_t)BAT_POWEROFF_WAIT_MS) delay(10);   // once only, when the user chooses Off
    DLOGM("BAT", "still powered (USB or the hold pin does not control power): hold restored\n");
    digitalWrite(BAT_HOLD_PIN, HIGH);
    g_batHold = true;
#else
    DLOGM("BAT", "power off: BAT_HOLD_ENABLE = 0, not possible\n");
#endif
    return false;
}

static int batteryMv() { LOCK(); const int v = g_batMv; UNLOCK(); return v; }
static int batteryChargePin() { LOCK(); const int v = g_batChg; UNLOCK(); return v; }

// One reading: BATTERY_AVG samples averaged. Stores the result under LOCK().
static void batterySampleNow() {
    long sum = 0;
    int lo = 100000, hi = 0;
    for (int i = 0; i < BATTERY_AVG; i++) {
        const int v = (int)analogReadMilliVolts(BAT_ADC_PIN);
        sum += v;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    const int avg = (int)(sum / BATTERY_AVG);
    const int chg = digitalRead(BAT_CHG_PIN);
    LOCK();
    g_batAdcMv = avg;
    g_batNoiseMv = hi - lo;
    g_batMv = avg * BAT_ADC_MULT;
    g_batChg = chg;
    UNLOCK();
}

static void batteryLog() {
    LOCK();
    const int adc = g_batAdcMv, noise = g_batNoiseMv, mv = g_batMv, chg = g_batChg;
    UNLOCK();
    (void)adc; (void)noise; (void)mv; (void)chg;   // DLOGM is empty when SERIAL_DEBUG = 0
    DLOGM("BAT", "adc GPIO%d %d mV (noise %d) -> battery %d mV (x%d ASSUMED) | charge pin GPIO%d = %s | hold GPIO%d = %s\n",
          (int)BAT_ADC_PIN, adc, noise, mv, (int)BAT_ADC_MULT, (int)BAT_CHG_PIN, chg ? "HIGH" : "LOW", (int)BAT_HOLD_PIN,
          g_batHold ? "HIGH" : "not driven");
}

static void batteryBegin() {
    analogSetPinAttenuation(BAT_ADC_PIN, ADC_11db);   // 0 .. ~3.1 V at the pin; battery 4.2 V / 3 = 1.4 V fits
    pinMode(BAT_CHG_PIN, INPUT);
    batterySampleNow();
    batteryLog();
}

// Registered in the 100 ms tick list: cheap when nothing is due
static void batteryTick() {
    static uint32_t lastSample = 0, lastLog = 0;
    static int lastChg = -1;
    const uint32_t now = millis();
    if ((uint32_t)(now - lastSample) < (uint32_t)BATTERY_SAMPLE_MS) return;
    lastSample = now;
    batterySampleNow();
    const int chg = batteryChargePin();
    if (lastChg >= 0 && chg != lastChg) {
        DLOGM("BAT", "charge pin GPIO%d changed: %s -> %s\n", (int)BAT_CHG_PIN, lastChg ? "HIGH" : "LOW", chg ? "HIGH" : "LOW");
        lastLog = now - (uint32_t)BATTERY_LOG_MS;   // log the full line right now
    }
    lastChg = chg;
    if ((uint32_t)(now - lastLog) >= (uint32_t)BATTERY_LOG_MS) {
        lastLog = now;
        batteryLog();
    }
}
