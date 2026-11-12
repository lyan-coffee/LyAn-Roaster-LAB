/*
 * GPL v3
 * Đọc lệnh HiBean (Skywalker V1 và V2): OT1 Heater, OT2 Air, DRUM, COOL, PID, READ...
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterState.h"
#include "RoasterControl.h"

// Hàng đợi lệnh
#define DIALECT_QUEUE_MAX 16
#define DIALECT_MSG_MAX   64
static char    g_dq[DIALECT_QUEUE_MAX][DIALECT_MSG_MAX];
static uint8_t g_dqHead = 0, g_dqTail = 0;

typedef void (*DialectReplyFn)(const String &);
static DialectReplyFn g_dialectReplyFn = nullptr;
static void dialectSetReply(DialectReplyFn fn) { g_dialectReplyFn = fn; }
static void dialectReply(const String &m) { if (g_dialectReplyFn) g_dialectReplyFn(m); }

// READ: trả "0,BT,BT,heat,vent"
static void handleREAD() {
    LOCK();
    double t = (CorF == 'F') ? (temp * 1.8 + 32.0) : temp;
    String msg = "0," + String(t, 1) + "," + String(t, 1) + "," +
                 String(sendBuffer[HEAT_BYTE]) + "," + String(sendBuffer[VENT_BYTE]) + "\n";
    UNLOCK();
    dialectReply(msg);
}

static void handleCHAN() { dialectReply("# Active channels set to 2100\n"); }

// Vị trí dấu ; hoặc , đầu tiên (-1 nếu không có)
static int dialectSep(const String &s) {
    const int a = s.indexOf(';'), b = s.indexOf(',');
    if (a < 0) return b;
    if (b < 0) return a;
    return a < b ? a : b;
}

static void parseAndExecuteCommands(String input) {
    input.trim();
    input.toUpperCase();
    g_lastHiBeanMs = millis();

    int split1 = dialectSep(input);
    String command = "", param = "", subcommand = "";

    if (split1 >= 0) {
        command = input.substring(0, split1);
        String remainder = input.substring(split1 + 1);
        int split2 = dialectSep(remainder);
        if (split2 >= 0) {
            subcommand = remainder.substring(0, split2);
            param = remainder.substring(split2 + 1);
        } else {
            param = remainder;
        }
    } else {
        command = input;
    }
    command.trim(); subcommand.trim(); param.trim();   // V2 có thể gửi "CLDN, ON"

    if (command == "PID") {
        if (param == "ON") {
            setPIDMode(true);
        } else if (param == "OFF") {
            setPIDMode(false);
        } else if (subcommand == "SV") {
            double sv = param.toDouble();
            if (CorF == 'F') sv = (sv - 32.0) / 1.8;      // đổi về °C
            if (sv > 0 && sv <= 300) { LOCK(); pSetpoint = sv; UNLOCK(); }
        } else if (subcommand == "T") {
            double v[3] = { myPIDConfig.getKp(), myPIDConfig.getKi(), myPIDConfig.getKd() };
            int n = 0;
            while (param.length() > 0 && n < 3) {
                int index = dialectSep(param);
                if (index == -1) { v[n++] = param.toDouble(); break; }
                v[n++] = param.substring(0, index).toDouble();
                param = param.substring(index + 1);
            }
            LOCK();
            myPIDConfig.setKp(v[0]); myPIDConfig.setKi(v[1]); myPIDConfig.setKd(v[2]);
            myPIDConfig.apply(myPID);
            UNLOCK();
        } else if (subcommand == "PM") {
            LOCK();
            myPIDConfig.setPMode(param == "M" ? P_ON_M : P_ON_E);
            myPIDConfig.apply(myPID);
            UNLOCK();
        } else if (subcommand == "CT") {
            LOCK();
            myPIDConfig.setSampleTime(param.toInt());
            myPIDConfig.apply(myPID);
            UNLOCK();
        }
    } else if (command == "OT1") {
        handleOT1(param.toInt(), SRC_HIBEAN);
    } else if (command == "READ") {
        handleREAD();
    } else if (command == "OT2") {
        handleVENT(param.toInt());
    } else if (command == "OFF") {
        roasterShutdown();
    } else if (command == "ESTOP") {
        eStop();
    } else if (command == "DRUM") {
        handleDRUM(param.toInt());
    } else if (command == "FILTER") {
        handleFILTER(param.toInt());
    } else if (command == "COOL") {
        handleCOOL(param.toInt());
    } else if (command == "OT3") {
        handleDRUM(param.toInt() > 0 ? 1 : 0);            // V1 chỉ bật / tắt Drum
    } else if (command == "CLDN") {
        handleCOOL(param == "ON" ? 100 : (param == "OFF" ? 0 : (int)param.toInt()));
    } else if (command == "CHAN") {
        handleCHAN();
    } else if (command == "UNITS") {
        if (split1 >= 0) {
            char u = input.charAt(split1 + 1);
            if (u == 'C' || u == 'F') CorF = u;
        }
    }
}

// BLE đẩy lệnh vào, loop() lấy ra
static void dialectPush(const String &s) {
    LOCK();
    uint8_t next = (g_dqHead + 1) % DIALECT_QUEUE_MAX;
    if (next != g_dqTail) {                                // đầy thì bỏ lệnh mới
        strncpy(g_dq[g_dqHead], s.c_str(), DIALECT_MSG_MAX - 1);
        g_dq[g_dqHead][DIALECT_MSG_MAX - 1] = 0;
        g_dqHead = next;
    }
    UNLOCK();
}

static void dialectDrain() {
    for (;;) {
        String msg;
        LOCK();
        bool has = (g_dqTail != g_dqHead);
        if (has) { msg = g_dq[g_dqTail]; g_dqTail = (g_dqTail + 1) % DIALECT_QUEUE_MAX; }
        UNLOCK();
        if (!has) break;
        parseAndExecuteCommands(msg);
    }
}
