/*
 * lib/RoasterCue.h - "Cricket chirp" cues: screen off / IDLE screen / screen on (tier 1, standalone)
 *
 * Only generates the sound samples (no I2S, no UI, no LVGL / FreeRTOS). Playback is done by
 * RoasterAlarmSound.h (ES8311 codec speaker or buzzer). To change pitch, length or volume, edit the flags below
 * (or #define them in RoasterSetting.h before this file is included).
 *
 *   cueParam(id)        frequency / length / volume of a cue
 *   cueSampleAt(id, i)  sample i (16 kHz, mono, int16) of cue id; 0 after the end
 *   cueGateAt(id, i)    true if sample i is inside a pulse (for the on/off buzzer)
 *
 * Chirp structure: each chirp has CUE_PULSES pulses of CUE_PULSE_MS, CUE_PULSE_GAP_MS between pulses,
 * CUE_CHIRP_GAP_MS between chirps; repeats until the cue length, then fades out over CUE_FADEOUT_MS.
 * Carrier lowered to 2.4 - 3.2 kHz for the small on-board speaker.
 *
 * Chosen: screen off + IDLE + power off / deep sleep = R3 (2400 Hz); screen on + Welcome = R2 (3200 Hz).
 */
#pragma once
#include <stdint.h>
#include <math.h>
#include "../RoasterSetting.h"

// ---- Whole module on/off (0 = silent) ----
#ifndef CUE_ENABLE
  #ifdef UI_SLEEP_SOUND
    #define CUE_ENABLE UI_SLEEP_SOUND
  #else
    #define CUE_ENABLE 1
  #endif
#endif

// ---- Per cue: frequency (Hz), length (ms, 0 = disabled), volume (0..100 %) ----
#ifndef CUE_SLEEP_FREQ_HZ
  #define CUE_SLEEP_FREQ_HZ 2400      // PWR press, screen off (HOME page, IDLE)
#endif
#ifndef CUE_SLEEP_MS
  #define CUE_SLEEP_MS      1200      // also the delay before the backlight turns off
#endif
#ifndef CUE_SLEEP_VOL
  #define CUE_SLEEP_VOL     40
#endif
#ifndef CUE_IDLE_FREQ_HZ
  #define CUE_IDLE_FREQ_HZ  2400      // enter IDLE screen (Logo page)
#endif
#ifndef CUE_IDLE_MS
  #define CUE_IDLE_MS       1200
#endif
#ifndef CUE_IDLE_VOL
  #define CUE_IDLE_VOL      40
#endif
#ifndef CUE_WAKE_FREQ_HZ
  #define CUE_WAKE_FREQ_HZ  3200      // screen back on
#endif
#ifndef CUE_WAKE_MS
  #define CUE_WAKE_MS       1200
#endif
#ifndef CUE_WAKE_VOL
  #define CUE_WAKE_VOL      40
#endif

#ifndef CUE_WELCOME_FREQ_HZ
  #define CUE_WELCOME_FREQ_HZ 3200    // Welcome page (power on / wake from deep sleep)
#endif
#ifndef CUE_WELCOME_MS
  #define CUE_WELCOME_MS    1200
#endif
#ifndef CUE_WELCOME_VOL
  #define CUE_WELCOME_VOL   40
#endif
// Boot chirp: CUE_BOOT_DELAY_MS after Welcome appears, CUE_BOOT_COUNT times, CUE_BOOT_GAP_MS apart (chirp length excluded)
#ifndef CUE_BOOT_DELAY_MS
  #define CUE_BOOT_DELAY_MS 3000      // first chirp 3 s after Welcome
#endif
#ifndef CUE_BOOT_COUNT
  #define CUE_BOOT_COUNT    3         // 3 chirps
#endif
#ifndef CUE_BOOT_GAP_MS
  #define CUE_BOOT_GAP_MS   1500      // 1.5 s pause before the 2nd and 3rd (from the end of the previous)
#endif
// IDLE screen (Logo page): after the entry chirp, chirp again every CUE_IDLE_REPEAT_MS until IDLE exits (0 = off)
#ifndef CUE_IDLE_REPEAT_MS
  #define CUE_IDLE_REPEAT_MS 300000UL // 5 minutes
#endif

// ---- Chirp shape (all cues) ----
#ifndef CUE_PULSES
  #define CUE_PULSES        4         // pulses per chirp
#endif
#ifndef CUE_PULSE_MS
  #define CUE_PULSE_MS      23        // pulse length
#endif
#ifndef CUE_PULSE_GAP_MS
  #define CUE_PULSE_GAP_MS  16        // gap after each pulse
#endif
#ifndef CUE_CHIRP_GAP_MS
  #define CUE_CHIRP_GAP_MS  230       // gap between chirps
#endif
#ifndef CUE_ATTACK_MS
  #define CUE_ATTACK_MS     4         // pulse attack ramp
#endif
#ifndef CUE_RELEASE_MS
  #define CUE_RELEASE_MS    9         // pulse release ramp
#endif
#ifndef CUE_DECAY_PCT
  #define CUE_DECAY_PCT     100       // each chirp's level as % of the previous (100 = constant, 80 = fading)
#endif
#ifndef CUE_FADEOUT_MS
  #define CUE_FADEOUT_MS    80        // fade-out at the end, avoids a click
#endif

#define CUE_SAMPLE_RATE 16000         // must equal ALARM_SAMPLE_RATE

enum { CUE_NONE = 0, CUE_SLEEP = 1, CUE_IDLE = 2, CUE_WAKE = 3, CUE_WELCOME = 4 };

struct CueParam { uint16_t freq; uint16_t ms; uint8_t vol; };

static inline CueParam cueParam(uint8_t id) {
    switch (id) {
        case CUE_SLEEP: return { (uint16_t)CUE_SLEEP_FREQ_HZ, (uint16_t)CUE_SLEEP_MS, (uint8_t)CUE_SLEEP_VOL };
        case CUE_IDLE:  return { (uint16_t)CUE_IDLE_FREQ_HZ,  (uint16_t)CUE_IDLE_MS,  (uint8_t)CUE_IDLE_VOL  };
        case CUE_WAKE:  return { (uint16_t)CUE_WAKE_FREQ_HZ,  (uint16_t)CUE_WAKE_MS,  (uint8_t)CUE_WAKE_VOL  };
        case CUE_WELCOME: return { (uint16_t)CUE_WELCOME_FREQ_HZ, (uint16_t)CUE_WELCOME_MS, (uint8_t)CUE_WELCOME_VOL };
        default:        return { 0, 0, 0 };
    }
}

#define CUE_MS2S(ms) ((uint32_t)(ms) * (uint32_t)CUE_SAMPLE_RATE / 1000UL)

static inline uint32_t cueTotalSamples(uint8_t id) {
#if CUE_ENABLE
    return CUE_MS2S(cueParam(id).ms);
#else
    (void)id; return 0;
#endif
}

// Position within the pulse (posInPulse) and chirp index (chirpIdx). false = resting.
static inline bool cuePulsePos(uint32_t i, uint32_t &posInPulse, uint32_t &chirpIdx) {
    const uint32_t ps = CUE_MS2S(CUE_PULSE_MS), unit = ps + CUE_MS2S(CUE_PULSE_GAP_MS);
    const uint32_t period = (uint32_t)CUE_PULSES * unit + CUE_MS2S(CUE_CHIRP_GAP_MS);
    chirpIdx = i / period;
    const uint32_t r = i % period;
    if (r / unit >= (uint32_t)CUE_PULSES) return false;
    posInPulse = r % unit;
    return posInPulse < ps;
}

static inline bool cueGateAt(uint8_t id, uint32_t i) {
    const CueParam p = cueParam(id);
    if (!CUE_ENABLE || p.ms == 0 || p.vol == 0 || i >= cueTotalSamples(id)) return false;
    uint32_t pos, ck;
    return cuePulsePos(i, pos, ck);
}

static inline int16_t cueSampleAt(uint8_t id, uint32_t i) {
    const CueParam p = cueParam(id);
    const uint32_t total = cueTotalSamples(id);
    if (!CUE_ENABLE || p.ms == 0 || p.vol == 0 || i >= total) return 0;
    uint32_t pos, ck;
    if (!cuePulsePos(i, pos, ck)) return 0;

    const float PI_F = 3.14159265f;
    const uint32_t ps = CUE_MS2S(CUE_PULSE_MS), a = CUE_MS2S(CUE_ATTACK_MS), d = CUE_MS2S(CUE_RELEASE_MS);
    float env = 1.0f;
    if (a > 0 && pos < a)          env = 0.5f - 0.5f * cosf(PI_F * (float)pos / (float)a);
    if (d > 0 && pos >= ps - d)    env *= 0.5f + 0.5f * cosf(PI_F * (float)(pos - (ps - d)) / (float)d);
    float gain = 1.0f;
#if CUE_DECAY_PCT != 100
    gain = powf((float)CUE_DECAY_PCT / 100.0f, (float)ck);
#else
    (void)ck;
#endif
    const uint32_t fo = CUE_MS2S(CUE_FADEOUT_MS);
    if (fo > 0 && i + fo > total) gain *= (float)(total - i) / (float)fo;

    const float s = sinf(2.0f * PI_F * (float)p.freq * (float)pos / (float)CUE_SAMPLE_RATE);
    return (int16_t)(s * env * gain * (32767.0f * (float)p.vol / 100.0f));
}
