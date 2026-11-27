/*
 * machines/Skywalker/RoasterDialect.h - HiBean dialect for this machine family (tier 4)
 *
 * handleREAD(), handleCHAN(), parseAndExecuteCommands() (PID / OT1 / OT2 / READ / OFF / ESTOP / DRUM / FILTER / COOL / CHAN / UNITS).
 * Every command goes through the control core functions (handleOT1, handleVENT, setPIDMode...), so the safety rules apply equally.
 * Replies (READ, CHAN) go through dialectReply(); the BLE module registers the send function with dialectSetReply().
 * So the Dialect does not need to know BLE exists, and the Serial console can test HiBean command strings without BLE.
 * The command-string queue lives here: dialectPush() (called by BLE) and dialectDrain() (called by loop).
 *
 * HiBean model "V2" (same V1 roaster, same control frame to the machine):
 *   - HiBean V1 sends  OT1;50  OT2;60  DRUM;1  DRUM;0        (semicolon)
 *   - HiBean V2 sends  OT1,80  OT2,60  OT3,70  CLDN,OFF  and event buttons mix both separators: PID;SV;185  PID;ON  CLDN;OFF  OT2,10  PID;OFF  OT1,55
 *   So the separator after the command name (and between parameters) is ';' OR ','. Two V2 commands:
 *     OT3,x    Drum speed 0..100: > 0 Drum ON, 0 Drum OFF (the V1 machine has on / off only; the hot-drum rule still applies as for DRUM;0).
 *              The firmware sends 2 commands: Drum on / off byte (V1 machine uses it) + speed x (g_drumSpeed / drumSpeedCmd() in this file, V2-style machines use it, V1 ignores it)
 *     CLDN,x   Cool: ON / OFF (or 0..100), same as COOL;x. Every Cool command produces 2 forms: Cool byte (V1 machine) + coolOnCmd() ON/OFF (V2-style machines)
 *   Both V1 and V2 forms of the same command (OT1, OT2, PID, READ, OFF...) share ONE handler.
 * ----------------------------------------------------------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include <queue>
#include "../../RoasterSetting.h"
#include "../../lib/RoasterSync.h"
#include "../../lib/RoasterState.h"
#include "../../lib/RoasterControl.h"

#define HAS_MODULE_DIALECT 1

// ---- Command-string queue from HiBean and replies ----
#define DIALECT_QUEUE_MAX 32
static std::queue<String> g_dialectQ;
typedef void (*DialectReplyFn)(const String &);
static DialectReplyFn g_dialectReplyFn = nullptr;     // registered by the BLE module
static bool           g_dialectEcho = false;          // true: print replies to Serial (console)

static void dialectSetReply(DialectReplyFn fn) { g_dialectReplyFn = fn; }
static void dialectReply(const String &m) {
    if (g_dialectReplyFn) g_dialectReplyFn(m);
    if (g_dialectEcho) DLOG("[HiBean <-] %s", m.c_str());   // log convention: "<-" = read from the roaster (READ reply, ...); "->" = command sent to the roaster (printed in RoasterBLE.h)
}

// -----------------------------------------------------------------------------
// Reply to READ for HiBean: "0,BT,BT,heat,vent"  (heat/vent are the machine's REAL values,
// so a change on the screen shows up in HiBean at the next READ)
// -----------------------------------------------------------------------------
void handleREAD() {
    LOCK();
    double t = (CorF == 'F') ? (temp * 1.8 + 32.0) : temp;
    String msg = "0," + String(t, 1) + "," + String(t, 1) + "," +
                 String(sendBuffer[HEAT_BYTE]) + "," + String(sendBuffer[VENT_BYTE]) + "\n";
    UNLOCK();
    dialectReply(msg);
}

void handleCHAN() {
    dialectReply("# Active channels set to 2100\n");
}

// -----------------------------------------------------------------------------
// "Send 2 commands" for Drum and Cool (HiBean V1 and V2): EVERY Drum / Cool command produces TWO forms; each machine uses the one it understands.
//   V1 form = byte in the frame to the machine (sendBuffer: Drum 0/100, Cool 0..100)  <- Skywalker V1 uses it, ignores the V2 form.
//   V2 form = drumSpeedCmd() (speed 0..100, 0 = Drum off) and coolOnCmd() (ON/OFF) <- V2-style machines use it, ignore the byte.
// Lives in the MACHINE FOLDER (does not touch the core): changing the roaster command set means changing the folder; the Control / State core stays the same.
// -----------------------------------------------------------------------------
static volatile uint8_t g_drumSpeed = 100;       // last requested Drum speed (1..100); DRUM;1 (V1 style) sets 100; turning Drum on from the screen / console reuses this value

// -----------------------------------------------------------------------------
// Separator: ';' (HiBean V1) or ',' (V2). Returns the position of the first one, -1 if none.
// -----------------------------------------------------------------------------
static int dialectSep(const String &s) {
    const int a = s.indexOf(';'), b = s.indexOf(',');
    if (a < 0) return b;
    if (b < 0) return a;
    return a < b ? a : b;
}

// -----------------------------------------------------------------------------
// Parse commands from HiBean
// -----------------------------------------------------------------------------
void parseAndExecuteCommands(String input) {
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
    command.trim(); subcommand.trim(); param.trim();   // HiBean V2 may send "CLDN, ON" / "PID; OFF" / "OT1, 0" (a space after the separator)

    if (command == "PID") {
        if (param == "ON") {
            setPIDMode(true);
        } else if (param == "OFF") {
            setPIDMode(false);
        } else if (subcommand == "SV") {
            double sv = param.toDouble();
            if (CorF == 'F') sv = (sv - 32.0) / 1.8;      // internally always °C
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
        if (param.toInt() != 0) g_drumSpeed = 100;      // V1-style command: on = speed 100
    } else if (command == "FILTER") {
        handleFILTER(param.toInt());
    } else if (command == "COOL") {
        handleCOOL(param.toInt());
    } else if (command == "OT3") {
        // HiBean V2 "2 commands": V1-style command (Drum on / off -> Drum byte to the machine) + V2-style command (speed x). The V1 machine takes the Drum command, ignores the speed.
        int x = (int)param.toInt();
        if (x < 0) x = 0;
        if (x > 100) x = 100;
        handleDRUM(x > 0 ? 1 : 0);
        if (x > 0) g_drumSpeed = (uint8_t)x;            // off (x = 0) keeps the old speed; hot-drum lock keeps the old speed too
    } else if (command == "CLDN") {
        handleCOOL(param == "ON" ? 100 : (param == "OFF" ? 0 : (int)param.toInt()));   // HiBean V2: Cool ON / OFF
    } else if (command == "CHAN") {
        handleCHAN();
    } else if (command == "UNITS") {
        if (split1 >= 0) {
            char u = input.charAt(split1 + 1);
            if (u == 'C' || u == 'F') CorF = u;
        }
    }
}

// ---- Queue: BLE (or console) pushes, loop() drains ----
static void dialectPush(const String &s) {
    LOCK();
    if (g_dialectQ.size() < DIALECT_QUEUE_MAX) g_dialectQ.push(s);
    UNLOCK();
}

// Step 3 of loop(): commands from HiBean
static void dialectDrain() {
    for (;;) {
        String msg;
        LOCK();
        bool has = !g_dialectQ.empty();
        if (has) { msg = g_dialectQ.front(); g_dialectQ.pop(); }
        UNLOCK();
        if (!has) break;
        parseAndExecuteCommands(msg);
    }
}
