/*
 * lib/RoasterControl.h - Roaster control core + safety (tier 3)
 *
 * sendBuffer is the single source of truth for Heater / Air / Drum / Cool. The screen (panel*), HiBean and Artisan all edit it through the functions here.
 *   - These functions only edit the buffer; sending the frame to the roaster is done by the machine DRIVER (machines/<machine>/RoasterMachine.h), called from loop().
 *   - Machine protocol (bit-bang, temperature parser) is in the driver; HiBean command parsing in RoasterDialect.h;
 *     batch + RoR in RoasterBatch.h; PID in RoasterPID.h; the CHECK byte is computed by the driver, not here.
 *
 * SAFETY RULES (RoasterSetting.h section 5):
 *   1. BT >= DRUM_AUTO_ON_C: Drum turns ON by itself; Drum-off commands are ignored until BT drops below it (handleDRUM, roasterShutdown, safetyWatchdog).
 *   2. Temperature signal lost (TEMP_STALE_MS) while heating (Heater > 0, PID on or roasting): g_signalLostAlarm = true,
 *      Heater CUT to 0% and PID off, Air unchanged; Heater / PID on commands are refused ("No temp signal") until the signal returns.
 *      When the signal returns the alarm closes by itself, Heater stays 0%. ENABLE_SIGNAL_LOST_ALARM = 0: message only, PID goes manual and holds Heater.
 *   Other safety: overheat cuts Heater; Air low / Drum OFF / Cool ON only WARN.
 *
 * Also here: controlInit(), controlExecute() + controlDrainQueue() (command queue: postCommand), controlSyncState() (writes g_state
 * for other modules and Web / OTA), panelSignalLostAck() (OK button on the alert), controlResetSignalFlags() (self-test).
 *
 * Display hint: max 23 characters.
 * Depends on: RoasterSetting.h, RoasterSync.h, RoasterState.h, RoasterBatch.h, RoasterPID.h. Knows nothing of the machine, BLE, network or UI.
 * ----------------------------------------------------------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"
#include "RoasterState.h"
#include "RoasterBatch.h"
#include "RoasterPID.h"

#define HAS_MODULE_CONTROL 1

enum Src : uint8_t { SRC_PANEL = 0, SRC_HIBEAN = 1 };

// Secondary state (protected by LOCK). roasting and batch: RoasterBatch.h. temp, lastTempMs: RoasterState.h
static Src      heatSrc       = SRC_PANEL;   // who controls the Heater
static char     hintBuf[24]   = "";
static uint32_t hintUntil     = 0;

// ---- prototypes ----
static void cutHeat(const char *why);
#define AIR_LOW_MSG "Need AIR >= " CFG_STR(MIN_AIR_FOR_HEAT) "%"   // the % comes from MIN_AIR_FOR_HEAT
static void enforceInterlock();
void handleHEAT(int value);

// -----------------------------------------------------------------------------
// Tien ich
// -----------------------------------------------------------------------------
static void setHint(const char *t) {
    LOCK();
    strncpy(hintBuf, t, sizeof(hintBuf) - 1);
    hintBuf[sizeof(hintBuf) - 1] = 0;
    hintUntil = millis() + 2500;
    UNLOCK();
}

// Only writes the bytes. The CHECK byte (sum of the first 5) is computed by the roaster DRIVER when sending the frame.
static void setValue(uint8_t *bytePtr, uint8_t value) {
    LOCK();
    *bytePtr = value;
    UNLOCK();
}

// Reason Heater is blocked (nullptr = allowed). START is required only for commands from the screen.
static const char *heatBlockReason(Src src) {
    if (evDone & (1 << EV_DROP)) return "Press END";                 // after Drop: Heater locked until END (same as HiBean)
#if ENABLE_SIGNAL_LOST_ALARM
    if (tempSignalLost()) return "No temp signal";                   // temperature signal lost -> Heater / PID cannot be turned on (any command source)
#endif
    if (src == SRC_PANEL && !roasting) return needStartHint();
#if SAFETY_INTERLOCK
    if (sendBuffer[DRUM_BYTE] == 0)               return "Need DRUM ON";
    if (sendBuffer[COOL_BYTE] > 0)                return "Cool fan is ON";
    if (sendBuffer[VENT_BYTE] < MIN_AIR_FOR_HEAT) return AIR_LOW_MSG;
#endif
    return nullptr;
}

// Heater cut level on overheat: g_overheatCutC (RoasterState.h)

// Cut Heater + leave PID
static void cutHeat(const char *why) {
    LOCK();
    if (myPID.GetMode() == AUTOMATIC) myPID.SetMode(MANUAL);
    manualHeatLevel = 0;
    setValue(&sendBuffer[HEAT_BYTE], 0);
    if (why) setHint(why);
    UNLOCK();
}

static void enforceInterlock() {
    LOCK();
    bool heating = sendBuffer[HEAT_BYTE] > 0 || myPID.GetMode() == AUTOMATIC;
    if (heating) {
        // Heating with Air low / Drum OFF / Cool ON: WARN only, Heater is NOT cut.
        // Heater is cut only on Drop or END (or emergency stop / signal lost in the watchdog).
        const char *r = heatBlockReason(heatSrc);
        if (r) setHint(r);
    }
    UNLOCK();
}

// User pressed OK on the signal-lost alert: close it, but Heater stays blocked until the signal returns
static bool s_sigAcked = false;

// Watchdog:
//   - OVERHEAT: temperature >= warning level -> cut Heater IMMEDIATELY (to 0, leave PID), any command source (screen / HiBean / Artisan).
//   - DRUM: BT >= DRUM_AUTO_ON_C -> Drum turns ON by itself.
//   - SIGNAL LOST while heating: alarm, cut Heater, PID off, Air unchanged. Signal back: alarm closes by itself, Heater stays 0.
//    (ENABLE_SIGNAL_LOST_ALARM = 0: message only, PID goes manual and holds the Heater level.)
//   - Air low / Drum OFF / Cool ON: repeated reminder, Heater not cut. HiBean lost: warning, PID goes manual and holds the Heater level.
static void safetyWatchdog() {
    LOCK();
    const uint32_t now = millis();
    bool heating = sendBuffer[HEAT_BYTE] > 0 || myPID.GetMode() == AUTOMATIC;
    if (g_overheatCutC > 0 && lastTempMs != 0 && (now - lastTempMs) < TEMP_SIGNAL_OK_MS && temp >= (double)g_overheatCutC && heating) {
        cutHeat("Overheat - Heater OFF");
        heating = false;
    }
#if DRUM_AUTO_ON_C > 0
    if (temp >= (double)DRUM_AUTO_ON_C && sendBuffer[DRUM_BYTE] == 0) {
        setValue(&sendBuffer[DRUM_BYTE], 100);
        setHint("Drum ON (BT>=" CFG_STR(DRUM_AUTO_ON_C) "C)");
    }
#endif
#if ENABLE_SIGNAL_LOST_ALARM
    if (!tempSignalLost()) {
        if (tempSignalOk()) { g_signalLostAlarm = false; s_sigAcked = false; }   // signal back: alarm closes by itself; Heater stays 0 until the user turns it on again
    } else {
        if (!g_signalLostAlarm && !s_sigAcked && (heating || roasting)) {
            g_signalLostAlarm = true;
            DLOG("[SAFETY] Mat tin hieu nhiet do khi dang cap nhiet -> canh bao, cat Heater, tat PID, giu Air\n");
        }
        if (heating) { cutHeat("No temp - Heater OFF"); heating = false; }
    }
#endif
    if (heating) {
        { const char *r = heatBlockReason(heatSrc); if (r) setHint(r); }   // Air low / Drum OFF / Cool ON: repeated reminder, Heater not cut
        if (heatSrc == SRC_HIBEAN && (now - g_lastHiBeanMs) > HIBEAN_TIMEOUT_MS) {
            setHint("HiBean lost");                                 // HiBean lost -> text hint only, Heater NOT cut
            if (myPID.GetMode() == AUTOMATIC) pidOffKeepHeat();     // PID is no longer supervised: manual roast, KEEP the Heater level
        }
#if !ENABLE_SIGNAL_LOST_ALARM
        else if (TEMP_STALE_MS > 0 && (now - lastTempMs) > TEMP_STALE_MS) {
            setHint("No temp signal");
            if (myPID.GetMode() == AUTOMATIC) pidOffKeepHeat();   // manual roast, keep the Heater level
        }
#endif
    }
    UNLOCK();
}

// -----------------------------------------------------------------------------
// Command handler (shared by HiBean and the screen)
// -----------------------------------------------------------------------------
void handleHEAT(int value) {
    if (value >= 0 && value <= 100) setValue(&sendBuffer[HEAT_BYTE], (uint8_t)value);
}

// Loc (filter) suy ra tu Vent: 1-25 -> 4, 26-50 -> 3, 51-75 -> 2, 76-100 -> 1
static void syncFilterFromVent() {
    int v = sendBuffer[VENT_BYTE];
    int f = (v == 0) ? 0 : (4 - ((v - 1) * 4 / 100));
    setValue(&sendBuffer[FILTER_BYTE], (uint8_t)f);
}

void handleOT1(int value, Src src = SRC_HIBEAN) {
    LOCK();
    if (myPID.GetMode() == MANUAL) {
        int v = constrain(value, 0, 100);
        if (v > 0) {
            const char *r = heatBlockReason(src);
            if (r) { setHint(r); v = 0; }
            else if (src == SRC_HIBEAN && HIBEAN_AUTOSTART && !roasting) roastBegin();
        }
        manualHeatLevel = v;
        heatSrc = src;
        handleHEAT(v);
    } else {
        // PID running: OT1 = max power
        myPIDConfig.setMaxPower(constrain(value, 0, 100));
        myPIDConfig.apply(myPID);
    }
    UNLOCK();
}

void handleVENT(int value) {
    if (value < 0 || value > 100) return;
    LOCK();
    setValue(&sendBuffer[VENT_BYTE], (uint8_t)value);
    syncFilterFromVent();
    enforceInterlock();
    UNLOCK();
}

void handleFILTER(int value) {
    if (value >= 0 && value <= 4) setValue(&sendBuffer[FILTER_BYTE], (uint8_t)value);
}

// BT >= DRUM_AUTO_ON_C: Drum-off commands are ignored (on commands still accepted)
void handleDRUM(int value) {
    LOCK();
#if DRUM_AUTO_ON_C > 0
    if (value == 0 && temp >= (double)DRUM_AUTO_ON_C) {
        setHint("Drum ON (BT>=" CFG_STR(DRUM_AUTO_ON_C) "C)");
        value = 1;
    }
#endif
    setValue(&sendBuffer[DRUM_BYTE], value != 0 ? 100 : 0);
    enforceInterlock();
    UNLOCK();
}

void handleCOOL(int value) {
    if (value < 0 || value > 100) return;
    LOCK();
    setValue(&sendBuffer[COOL_BYTE], (uint8_t)value);
    // Cool is only a cooling fan: it does NOT end the batch, stop the clock or change anything else.
    // (The safety interlock still cuts Heater when Cool is ON - Heater cannot run with Cool.)
    if (value > 0) enforceInterlock();
    syncFilterFromVent();                            // filter follows Vent
    UNLOCK();
}

void eStop() {
    LOCK();
    cutHeat("E-STOP");
    handleVENT(100);
    UNLOCK();
}

// All off to 0, but Drum stays ON if BT >= DRUM_AUTO_ON_C (hot-drum rule)
void roasterShutdown() {
    LOCK();
    for (int i = 0; i < CONTROLLER_LENGTH; i++) sendBuffer[i] = 0;
#if DRUM_AUTO_ON_C > 0
    if (temp >= (double)DRUM_AUTO_ON_C) sendBuffer[DRUM_BYTE] = 100;
#endif
    if (myPID.GetMode() == AUTOMATIC) myPID.SetMode(MANUAL);
    manualHeatLevel = 0;
    roastEnd();
    UNLOCK();
}

void setPIDMode(bool usePID) {
    LOCK();
    if (usePID) {
        heatSrc = SRC_HIBEAN;
        const char *r = heatBlockReason(SRC_HIBEAN);
        if (r) {
            setHint(r);
        } else {
            if (HIBEAN_AUTOSTART && !roasting) roastBegin();
            pInput  = temp;                          // bumpless start: PID begins from the current Heater power,
            pOutput = sendBuffer[HEAT_BYTE];         // do not use a stale PID value from before
            myPID.SetMode(AUTOMATIC);
        }
    } else {
        pidOffKeepHeat();                  // PID off: keep the current Heater level
    }
    UNLOCK();
}

// PID: runs in loop() (core 1), which calls it VERY often (much faster than the PID cycle PID_SAMPLE_MS).
// Important: pOutput is written only when myPID.Compute() actually produced a new value, otherwise Heater is
// pulled back to the old value between computations.
void handlePIDControl() {
    static bool farFromSet = false;                  // in the far-from-set zone (P_ON_E): the integral is held at PID_I_PRESET
    LOCK();
    if (myPID.GetMode() == AUTOMATIC) {
        pInput = temp;
        const double err = pSetpoint - temp;
        if (myPIDConfig.getPMode() == P_ON_E && err > PID_I_BAND_C) {
            // Far from Set: do not let the integral wind up to 100% (would overshoot Set). Heater = Kp*error + PID_I_PRESET (clamped 0..maxPower).
            farFromSet = true;
            const double hd = constrain(myPIDConfig.getKp() * err + PID_I_PRESET, 0.0, (double)myPIDConfig.getMaxPower());
            pOutput = hd;
            handleHEAT(constrain((int)(round(hd / 5.0) * 5), 0, 100));
        } else {
            if (farFromSet) {
                // Just entered the near-Set zone: load the integral = PID_I_PRESET ONCE (Initialize(): outputSum = pOutput, lastInput = pInput)
                farFromSet = false;
                pOutput = PID_I_PRESET;
                myPID.SetMode(MANUAL);
                myPID.SetMode(AUTOMATIC);
            }
            if (myPID.Compute()) {                   // true only when a full sample time has elapsed
                handleHEAT(constrain((int)(round(pOutput / 5.0) * 5), 0, 100));
            }
        }
    } else {
        farFromSet = false;
    }
    UNLOCK();
}

// -----------------------------------------------------------------------------
// Actions from the touch screen
// -----------------------------------------------------------------------------
void panelSetHeater(int v) {
    v = constrain(v, 0, 100);
    v = ((v + 2) / 5) * 5;
    LOCK();
    if (myPID.GetMode() == AUTOMATIC) pidOffKeepHeat();     // manual change = switch to manual, Heater follows the value just set (not back to 0)
    handleOT1(v, SRC_PANEL);
    UNLOCK();
}
// PID on/off from the screen (can be turned on only under the same safety conditions as Heater)
void panelSetPID(bool on) {
    LOCK();
    if (on) {
        const char *r = heatBlockReason(SRC_PANEL);
        if (r) {
            setHint(r);
        } else {
            if (pSetpoint <= 0) pSetpoint = DEFAULT_PID_SETPOINT;
            heatSrc = SRC_PANEL;
            pInput  = temp;                          // bumpless start
            pOutput = sendBuffer[HEAT_BYTE];
            myPID.SetMode(AUTOMATIC);
        }
    } else {
        pidOffKeepHeat();                 // PID off: keep the current Heater level, go manual
    }
    UNLOCK();
}
// Set temperature (SV), °C
void panelSetSetpoint(float c) {
    LOCK();
    pSetpoint = constrain((double)(round(c / 5.0f) * 5), 0.0, 300.0);   // bo 5 do C
    UNLOCK();
}
// Cut Heater now (and PID off) - for the "Heater Off" button on the overheat alert
void panelHeaterOff() {
    LOCK();
    cutHeat(nullptr);
    UNLOCK();
}
void panelSetAir(int v)     { handleVENT(constrain(v, 0, 100)); }
void panelSetDrum(bool on)  { handleDRUM(on ? 100 : 0); }
void panelSetCool(bool on)  { handleCOOL(on ? 100 : 0); }

void panelStart() {
    LOCK();
    if (batchLive) { setHint("Press END"); UNLOCK(); return; }   // a batch is open: END first
    batchReset();                      // START = begin a new batch: clear events and the Time clock
    handleCOOL(0);
    setValue(&sendBuffer[DRUM_BYTE], 100);
    if (sendBuffer[VENT_BYTE] < MIN_AIR_FOR_HEAT) handleVENT(MIN_AIR_FOR_HEAT);
    roastBegin();
    UNLOCK();
}

// END: finish the batch and clear everything (graph, events, clock). Before Drop: 1st press -> 5 s confirmation, 2nd -> done.
// After Drop: ends at once, no confirmation. Cool / Drum / Air unchanged (beans may still be cooling).
void panelEnd() {
    LOCK();
    const uint32_t now = millis();
    if (!batchLive && !roasting) { UNLOCK(); return; }
    const bool dropped = (evDone & (1 << EV_DROP)) != 0;
    if (!dropped && !(endArmUntil != 0 && (int32_t)(endArmUntil - now) > 0)) {
        endArmUntil = now + END_CONFIRM_MS;
        UNLOCK();
        return;
    }
    roastEnd();
    cutHeat(nullptr);
    batchReset();
    batchLive = false;
    UNLOCK();
}

// Event buttons PreHeat / Charge / DE / 1FC / 2FC (Drop has its own function: panelDrop)
void panelEvent(int ev) {
    if (ev < 0 || ev >= EV_COUNT || ev == EV_DROP) return;
    LOCK();
    if (evDone & (1 << ev)) { UNLOCK(); return; }             // event already pressed
    if (!roasting) { setHint(needStartHint()); UNLOCK(); return; }   // only while roasting (START pressed)
    if (ev == EV_PREHEAT) {
        if (batchCharged) { UNLOCK(); return; }               // PreHeat only before Charge
        evDone |= (1 << ev);
#if ENABLE_EVENT_PRESETS
        // PR preset: set Air and enable PID at preheat target
        handleVENT(PRESET_PR_AIR);
        pSetpoint = g_state.tempUnitF ? (PRESET_PR_TEMP_F - 32) / 1.8 : (double)PRESET_PR_TEMP_C;
        panelSetPID(true);
#endif
    } else if (ev == EV_CHARGE) {
        batchCharged = true; batchRunning = true;
        chargeMs = bclock(); batchFrozenMs = 0; evMs[EV_CHARGE] = 0; evTemp[EV_CHARGE] = (float)temp;
        evDone |= (1 << ev);
#if ENABLE_EVENT_PRESETS
        // CH preset: switch to manual heater and air (set air first for safety interlock)
        pidOffKeepHeat();
        handleVENT(PRESET_CH_AIR);
        panelSetHeater(PRESET_CH_HEATER);
#endif
    } else {
        if (!batchRunning) { setHint("Press Charge"); UNLOCK(); return; }
        evMs[ev] = bclock() - chargeMs; evTemp[ev] = (float)temp;
        evDone |= (1 << ev);
    }
    UNLOCK();
}

// Drop: allowed once START is pressed, but 2FC is NOT required (even before Charge).
// 1st press = wait for confirmation (cancels after 5 s); 2nd press = drop the beans:
// Cool on, Heater off (leave PID), stop the Time clock.
// If the Time clock is not running (no Charge), Drop records no time (EV_NO_TIME) - the UI shows no time.
void panelDrop() {
    LOCK();
    const uint32_t now = millis();
    if (evDone & (1 << EV_DROP)) {
        // already dropped: wait for a new START
    } else if (!roasting) {
        setHint(needStartHint());
    } else if (dropArmUntil != 0 && (int32_t)(dropArmUntil - now) > 0) {
        dropArmUntil = 0;
        evMs[EV_DROP] = batchRunning ? (bclock() - chargeMs) : EV_NO_TIME; evTemp[EV_DROP] = (float)temp;
        evDone |= (1 << EV_DROP);
        roastEnd();                                           // Drop: stop the clock + cut Heater, Cool on, Air max
        cutHeat(nullptr);
        handleCOOL(100);
        handleVENT(100);
    } else {
        dropArmUntil = now + DROP_CONFIRM_MS;
    }
    UNLOCK();
}

// User pressed OK on the signal-lost alert -> close it (Heater stays blocked until the signal returns)
void panelSignalLostAck() {
    LOCK();
    g_signalLostAlarm = false;
    s_sigAcked = true;
    UNLOCK();
}

// -----------------------------------------------------------------------------
// Snapshot of the state for the UI
// -----------------------------------------------------------------------------
struct Snap {
    int   heat, vent, drum, cool;
    bool  roasting, pid, tempOk, sim;
    float temp, sp;
    uint32_t elapsed;
    char  hint[24];
    // Time / RoR / batch events
    uint32_t batchMs;           // dong ho Time (ms)
    bool     batchRun, charged;
    bool     rorOk;
    float    ror;               // °C / min
    uint8_t  evDone;
    uint32_t evMs[EV_COUNT];
    float    evTemp[EV_COUNT];   // BT (°C) when the event was pressed
    uint32_t dropArmMs;         // > 0: ms left to confirm Drop
    bool     live;              // batch open (START ... END)
    uint32_t endArmMs;          // > 0: ms left to confirm END
    bool     sigLostAlarm;      // signal-lost alarm is open
    bool     drumHot;           // BT >= DRUM_AUTO_ON_C: Drum is locked ON
};

static void roasterSnapshot(Snap &s) {
    LOCK();
    s.heat = sendBuffer[HEAT_BYTE];
    s.vent = sendBuffer[VENT_BYTE];
    s.drum = sendBuffer[DRUM_BYTE];
    s.cool = sendBuffer[COOL_BYTE];
    s.roasting = roasting;
    s.live = batchLive;
    s.sim = g_simActive;
    s.pid = (myPID.GetMode() == AUTOMATIC);
    s.temp = (float)temp;
    s.sp = (float)pSetpoint;
    s.tempOk = (lastTempMs != 0) && ((millis() - lastTempMs) < TEMP_SIGNAL_OK_MS);
    s.elapsed = roastElapsedMs();
    s.batchMs = batchElapsedMs();
    s.batchRun = batchRunning;
    s.charged = batchCharged;
    s.rorOk = rorOk;
    s.ror = rorValue;
    s.evDone = evDone;
    for (int i = 0; i < EV_COUNT; i++) { s.evMs[i] = evMs[i]; s.evTemp[i] = evTemp[i]; }
    {
        uint32_t nowMs = millis();
        s.dropArmMs = (dropArmUntil != 0 && (int32_t)(dropArmUntil - nowMs) > 0) ? (dropArmUntil - nowMs) : 0;
        s.endArmMs  = (endArmUntil != 0 && (int32_t)(endArmUntil - nowMs) > 0) ? (endArmUntil - nowMs) : 0;
    }
    s.sigLostAlarm = g_signalLostAlarm;
    s.drumHot = (DRUM_AUTO_ON_C > 0) && (temp >= (double)DRUM_AUTO_ON_C);
    if (millis() < hintUntil) strncpy(s.hint, hintBuf, sizeof(s.hint)); else s.hint[0] = 0;
    s.hint[sizeof(s.hint) - 1] = 0;
    UNLOCK();
}

// -----------------------------------------------------------------------------
// Init + command queue + g_state sync
// -----------------------------------------------------------------------------
// Call once in setup(), AFTER syncInit(). (PID manual, roasterShutdown())
static void controlInit() {
    pidInit();
    g_state.tempUnitF = (DEFAULT_TEMP_UNIT == 1);
    g_overheatCutC = ENABLE_OVERHEAT_ALARM ? (uint16_t)DEFAULT_WARN_TEMP : (uint16_t)0;   // the Alarm module updates it from Setting
    roasterShutdown();                // Heater 0%, everything off
}

// Run ONE queued command (postCommand): same semantics as the screen / Artisan (panel*), so Heater needs START first.
//   CMD_DROP: cut Heater, keep Drum / Air -> panelHeaterOff().  CMD_ESTOP: eStop().
static void controlExecute(const RoasterCmd &c) {
    switch (c.type) {
        case CMD_SET_HEAT:   panelSetHeater((int)lroundf(c.p1)); break;
        case CMD_STEP_HEAT:  { Snap s; roasterSnapshot(s); panelSetHeater(constrain(s.heat + (int)lroundf(c.p1), 0, 100)); } break;
        case CMD_SET_AIR:    panelSetAir((int)lroundf(c.p1)); break;
        case CMD_STEP_AIR:   { Snap s; roasterSnapshot(s); panelSetAir(constrain(s.vent + (int)lroundf(c.p1), 0, 100)); } break;
        case CMD_SET_DRUM:   panelSetDrum(c.p1 > 0.0f); break;
        case CMD_SET_COOL:   panelSetCool(c.p1 > 0.0f); break;
        case CMD_PID_ON:     panelSetPID(true); break;
        case CMD_PID_OFF:    panelSetPID(false); break;
        case CMD_PID_SV:     panelSetSetpoint(c.p1); break;
        case CMD_PID_TUNE:
            LOCK();
            myPIDConfig.setKp(c.p1); myPIDConfig.setKi(c.p2); myPIDConfig.setKd(c.p3);
            myPIDConfig.apply(myPID);
            UNLOCK();
            break;
        case CMD_SET_UNIT_F: g_state.tempUnitF = (c.p1 > 0.0f); break;
        case CMD_DROP:       panelHeaterOff(); break;
        case CMD_ESTOP:      eStop(); break;
        default: break;
    }
}

// Called every loop() (core 1): drain the command queue
static void controlDrainQueue() {
    RoasterCmd c;
    while (popCommand(c)) controlExecute(c);
}

// Called every loop(): write the shared board (g_state) for other modules and for Web / OTA (roastBusy)
static void controlSyncState() {
    LOCK();
    g_state.bt = (float)temp;
    g_state.et = (float)temp;                       // no separate ET probe (MACHINE_HAS_ET = 0)
    g_state.ror = rorOk ? rorValue : 0.0f;
    g_state.pidSV = (float)pSetpoint;
    g_state.pidActive = (myPID.GetMode() == AUTOMATIC);
    g_state.simMode = g_simActive;
    g_state.rxFault = tempSignalLost();
    g_state.roastBusy = batchLive || roasting;      // Web / OTA must not restart or flash while roasting
    UNLOCK();
}
