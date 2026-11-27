/*
 * machines/Skywalker/RoasterMachine.h - Skywalker V1 socket: receive temperature, send the control frame (tier 3)
 *
 * Contents:
 *   - protocol timing constants + pulsePin() + sendRoasterFrame() (TX_PIN is the selected pin)
 *   - temperature receive (loop step 2) and pin init (end of setup)
 * Notes:
 *   - No delay() in setup(): machineInit() runs from loop()
 *     once millis() >= ROASTER_BOOT_DELAY_MS (so flashing finishes before GPIO 19/20 are taken from USB). No frames are sent until then.
 *   - The CHECK byte (sum of the first 5 bytes) is computed here when the frame is sent.
 *   - SERIAL_DEBUG = 1: TX/RX pins are not claimed, so no temperature is received and no frame is sent; machineSetTemp() still works
 *     for testing (Serial console: "t" / "hold").
 *   - No frame is sent while simulating (g_simActive), so a real roaster does not receive commands driven by a fake temperature.
 *
 * Interface for the rest of the program (every machine uses these names):
 *   machinePoll()      every loop(): receive a temperature frame, write temp / lastTempMs / lastRealTempMs
 *   machineSend()      every loop(): send one frame to the machine (~40-100 ms). Returns true if sent
 *   machineSetTemp(c)  record a temperature as if the machine had just sent it (used by poll, console, tests)
 *   machineChecksum()  compute and write the CHECK byte into sendBuffer, returns it
 *   machineSendOff()   send one "all off" frame (all zeros). Called once when simulation starts so a connected real roaster goes safe
 * ----------------------------------------------------------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "../../RoasterSetting.h"
#include "../../lib/RoasterSync.h"
#include "../../lib/RoasterState.h"
#include "RoasterParser.h"

#define HAS_MODULE_MACHINE 1

// Roaster protocol timing
const int PULSE_ZERO       = 650;
const int POST_PULSE_DELAY = 750;
const int START_PULSE      = 7500;
const int START_DELAY      = 3800;

static void pulsePin(int pin, int duration) {
    if (pin < 0) return;
    digitalWrite(pin, LOW);
    delayMicroseconds(duration);
    digitalWrite(pin, HIGH);
}

void sendRoasterFrame(const uint8_t *buf) {
    pulsePin(ROASTER_TX_PIN, START_PULSE);
    delayMicroseconds(START_DELAY);

    for (int i = 0; i < CONTROLLER_LENGTH; i++) {
        for (int j = 0; j < 8; j++) {
            pulsePin(ROASTER_TX_PIN, bitRead(buf[i], j) ? 1500 : PULSE_ZERO);
            delayMicroseconds(POST_PULSE_DELAY);
        }
    }
}

// ---------------------------------------------------------------- V3
static SkyRoasterParser g_roasterRx;               // receiver object
static bool     g_machineReady = false;            // pin initialized
static uint32_t g_machineFrames = 0;               // frames sent
static uint32_t g_machineRxOk = 0, g_machineRxBad = 0;   // valid temperature frames / checksum errors

// Record a temperature (°C) as if the roaster had just sent it
static void machineSetTemp(double c) {
    LOCK();
    temp = c;
    lastTempMs = millis();
    lastRealTempMs = lastTempMs;
    UNLOCK();
}

// Pin init (once). Called every loop(): does nothing until ROASTER_BOOT_DELAY_MS.
static void machineInit() {
    if (g_machineReady) return;
    if (millis() < (uint32_t)ROASTER_BOOT_DELAY_MS) return;
    if (ROASTER_TX_PIN >= 0) {
        pinMode(ROASTER_TX_PIN, OUTPUT);
        digitalWrite(ROASTER_TX_PIN, HIGH);
    }
    if (ROASTER_RX_PIN >= 0) {
        g_roasterRx.begin(ROASTER_RX_PIN);         // ISR attached on core 1
        g_roasterRx.enableDebug(false);
    }
    g_machineReady = true;
}

// Loop step 2: bean temperature from the roaster
// Only one sample is taken per TEMP_SAMPLE_MS (500 ms). A valid frame arriving earlier is still read out of the buffer but ignored (temp not written).
static uint32_t g_machineRxSkip = 0;               // valid frames skipped because it is not yet sample time
static uint32_t g_machineSampleMs = 0;             // time of the last accepted sample (millis)
static bool machineSampleDue(uint32_t now, uint32_t last, bool have) {
    return TEMP_SAMPLE_MS <= 0 || !have || (uint32_t)(now - last) >= (uint32_t)TEMP_SAMPLE_MS;
}
static void machinePoll() {
    machineInit();
    if (!g_machineReady || ROASTER_RX_PIN < 0) return;
    if (g_roasterRx.msgAvailable()) {
        uint8_t msg[7];
        g_roasterRx.getMessage(msg);
        if (g_roasterRx.validate(msg)) {
            g_machineRxOk++;
            const uint32_t now = millis();
            if (machineSampleDue(now, g_machineSampleMs, g_machineSampleMs != 0)) {
                g_machineSampleMs = now ? now : 1;       // 0 = no sample yet
                machineSetTemp(g_roasterRx.getTemperature(msg));
            } else {
                g_machineRxSkip++;
            }
        } else {
            g_machineRxBad++;
        }
    }
}

static uint8_t machineChecksum() {
    LOCK();
    uint8_t sum = 0;
    for (int i = 0; i < (CONTROLLER_LENGTH - 1); i++) sum += sendBuffer[i];
    sendBuffer[CHECK_BYTE] = sum;
    UNLOCK();
    return sum;
}

// Loop step 5: send the control frame to the machine (blocks ~40-100 ms). Returns true if sent.
static bool machineSend() {
    if (!g_machineReady || ROASTER_TX_PIN < 0) return false;
    if (g_simActive) return false;                 // simulating: do not send
    uint8_t buf[CONTROLLER_LENGTH];
    machineChecksum();
    LOCK();
    memcpy(buf, sendBuffer, CONTROLLER_LENGTH);
    UNLOCK();
    sendRoasterFrame(buf);
    g_machineFrames++;
    return true;
}

// Simulation just started -> put the real roaster to "all off", then stop sending (see machineSend)
static void machineSendOff() {
    if (!g_machineReady || ROASTER_TX_PIN < 0) return;
    static const uint8_t zero[CONTROLLER_LENGTH] = { 0, 0, 0, 0, 0, 0 };
    sendRoasterFrame(zero);
    g_machineFrames++;
}
