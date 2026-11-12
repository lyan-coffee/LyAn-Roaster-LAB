/*
 * GPL v3
 * Giao tiếp với máy Skywalker V1: nhận nhiệt độ, gửi khung lệnh.
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterState.h"

// Nhận khung nhiệt độ 7 byte từ máy (°C)
class SkyRoasterParser {
public:
    void begin(uint8_t pin);
    bool msgAvailable() { return newMessage; }
    void getMessage(uint8_t *dest);
    bool validate(const uint8_t *buf);
    double getTemperature(const uint8_t *buf);

private:
    static void IRAM_ATTR edgeISR();
    void handleEdge();

    int pin = -1;

    static const uint8_t  MSG_BYTES     = 7;
    static const uint8_t  BITS_PER_BYTE = 8;
    static const unsigned long START_MIN_US = 7000;
    static const unsigned long START_MAX_US = 10000;
    static const unsigned long BIT0_MAX_US  = 900;
    static const unsigned long BIT1_MIN_US  = 1200;
    static const unsigned long BIT1_MAX_US  = 2000;

    enum RxState { IDLE, RECEIVING };
    volatile RxState rxState = IDLE;

    volatile unsigned long lastEdgeTime = 0;
    volatile bool lastEdgeWasLow = false;

    uint8_t bitCount = 0;
    uint8_t byteIndex = 0;
    volatile uint8_t currentByte = 0;
    volatile uint8_t messageBuf[MSG_BYTES];
    volatile bool newMessage = false;

    static SkyRoasterParser *instance;
};

SkyRoasterParser* SkyRoasterParser::instance = nullptr;

void SkyRoasterParser::begin(uint8_t p) {
    instance = this;
    rxState = IDLE;
    pin = p;
    pinMode(p, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(p), SkyRoasterParser::edgeISR, CHANGE);
}

void SkyRoasterParser::getMessage(uint8_t *dest) {
    noInterrupts();
    for (uint8_t i = 0; i < MSG_BYTES; i++) dest[i] = messageBuf[i];
    newMessage = false;
    interrupts();
}

bool SkyRoasterParser::validate(const uint8_t *buf) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < MSG_BYTES - 1; i++) sum += buf[i];
    return (sum == buf[MSG_BYTES - 1]);
}

double SkyRoasterParser::getTemperature(const uint8_t *buf) {
    uint16_t rawTempX = ((buf[0] << 8) + buf[1]);
    uint16_t rawTempY = ((buf[2] << 8) + buf[3]);

    double x = 0.001 * rawTempX;
    double y = 0.001 * rawTempY;
    double temperature;

    if (rawTempX > 836 || rawTempY > 221) {
        temperature = -224.2 * y * y * y + 385.9 * y * y - 327.1 * y + 171;
    } else {
        temperature = -278.33 * x * x * x + 491.944 * x * x - 451.444 * x + 310.668;
    }
    return temperature;
}

void IRAM_ATTR SkyRoasterParser::edgeISR() {
    if (instance) instance->handleEdge();
}

void SkyRoasterParser::handleEdge() {
    unsigned long now = micros();
    bool pinIsLow = (digitalRead(digitalPinToInterrupt(this->pin)) == LOW);

    if (pinIsLow) {
        lastEdgeTime = now;
        lastEdgeWasLow = true;
    } else {
        if (!lastEdgeWasLow) return;
        unsigned long lowDur = now - lastEdgeTime;
        lastEdgeWasLow = false;

        switch (rxState) {
        case IDLE:
            if (lowDur >= START_MIN_US && lowDur <= START_MAX_US) {
                byteIndex = 0; bitCount = 0; currentByte = 0;
                rxState = RECEIVING;
            }
            break;

        case RECEIVING: {
            uint8_t bitVal = 0xFF;
            if (lowDur < BIT0_MAX_US) bitVal = 0;
            else if (lowDur >= BIT1_MIN_US && lowDur <= BIT1_MAX_US) bitVal = 1;
            else { rxState = IDLE; return; }

            currentByte |= (bitVal << bitCount);

            if (++bitCount >= BITS_PER_BYTE) {
                messageBuf[byteIndex++] = currentByte;
                currentByte = 0;
                bitCount = 0;
                if (byteIndex >= MSG_BYTES) {
                    newMessage = true;
                    rxState = IDLE;
                }
            }
            break;
        }
        }
    }
}

// Gửi khung lệnh (theo nhịp giao thức của máy)
static const int PULSE_ZERO       = 650;
static const int POST_PULSE_DELAY = 750;
static const int START_PULSE      = 7500;
static const int START_DELAY      = 3800;

static void pulsePin(int pin, int duration) {
    if (pin < 0) return;
    digitalWrite(pin, LOW);
    delayMicroseconds(duration);
    digitalWrite(pin, HIGH);
}

static void sendRoasterFrame(const uint8_t *buf) {
    pulsePin(ROASTER_TX_PIN, START_PULSE);
    delayMicroseconds(START_DELAY);

    for (int i = 0; i < CONTROLLER_LENGTH; i++) {
        for (int j = 0; j < 8; j++) {
            pulsePin(ROASTER_TX_PIN, bitRead(buf[i], j) ? 1500 : PULSE_ZERO);
            delayMicroseconds(POST_PULSE_DELAY);
        }
    }
}

static SkyRoasterParser g_roasterRx;
static bool     g_machineReady = false;

// Ghi một số đo nhiệt độ (°C)
static void machineSetTemp(double c) {
    LOCK();
    temp = c;
    lastTempMs = millis();
    if (lastTempMs == 0) lastTempMs = 1;           // 0 = chưa từng có tín hiệu
    UNLOCK();
}

// Khởi tạo chân sau ROASTER_BOOT_DELAY_MS
static void machineInit() {
    if (g_machineReady) return;
    if (millis() < (uint32_t)ROASTER_BOOT_DELAY_MS) return;
    if (ROASTER_TX_PIN >= 0) {
        pinMode(ROASTER_TX_PIN, OUTPUT);
        digitalWrite(ROASTER_TX_PIN, HIGH);
    }
    if (ROASTER_RX_PIN >= 0) g_roasterRx.begin(ROASTER_RX_PIN);
    g_machineReady = true;
}

// Đọc nhiệt độ từ máy
static void machinePoll() {
    machineInit();
    if (!g_machineReady || ROASTER_RX_PIN < 0) return;
    if (g_roasterRx.msgAvailable()) {
        uint8_t msg[7];
        g_roasterRx.getMessage(msg);
        if (g_roasterRx.validate(msg)) machineSetTemp(g_roasterRx.getTemperature(msg));
    }
}

// Gửi khung lệnh (~40-100 ms). CHECK = tổng 5 byte đầu.
static bool machineSend() {
    if (!g_machineReady || ROASTER_TX_PIN < 0) return false;
    uint8_t buf[CONTROLLER_LENGTH];
    LOCK();
    uint8_t sum = 0;
    for (int i = 0; i < CONTROLLER_LENGTH - 1; i++) sum += sendBuffer[i];
    sendBuffer[CHECK_BYTE] = sum;
    memcpy(buf, sendBuffer, CONTROLLER_LENGTH);
    UNLOCK();
    sendRoasterFrame(buf);
    return true;
}
