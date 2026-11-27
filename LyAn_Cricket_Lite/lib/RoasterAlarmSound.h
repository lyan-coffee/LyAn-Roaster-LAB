/*
 * lib/RoasterAlarmSound.h - Alarm sounds (overheat + temperature signal lost) (tier 1)
 *
 * Runs in its own task on core 0 (does not block the UI or the roaster).
 *
 *   ALARM_AUDIO_MODE 0: silent (alarm screen only)
 *   ALARM_AUDIO_MODE 1: board speaker (default): ESP32 -> I2S -> ES8311 codec -> NS4150B amplifier (PA pin on while sounding)
 *   ALARM_AUDIO_MODE 2: buzzer on GPIO ALARM_BUZZER_PIN
 *
 * The ES8311 codec needs:
 *   - an MCLK clock (GPIO ALARM_I2S_MCLK = 256 x 16 kHz),
 *   - one-time I2C setup at boot (ALARM_CODEC_ADDR). The I2C bus is SHARED with touch (Wire opened in touchBegin()),
 *     so alarmSoundStart() must be called from the touch (UI) task; the sound task never touches I2C afterwards,
 *   - the PA pin (ALARM_PA_PIN) on while sounding, off when silent to avoid hiss.
 *
 * Two distinct alarms:
 *   OVERHEAT (ALARM_SND_HEAT):  fast "bi-bi-bip", two tones 880 / 1320 Hz, ~0.4 s rest
 *   LOST (ALARM_SND_LOST):      two long falling tones 784 -> 523 Hz, ~0.4 s rest
 * Each step is 100 ms; 0 = silent. Changing the sound type restarts the pattern.
 *
 * Depends on: RoasterSetting.h (ALARM_* and speaker pins), RoasterSync.h (DLOGM). No LVGL.
 * If the codec does not answer on I2C (wrong address / pin), "[ALARM] codec ..." is logged and sound is skipped; the UI is unaffected.
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"
#include "RoasterCue.h"          // cricket chirp (screen off / on, IDLE): standalone, only generates samples

#ifndef ALARM_AUDIO_MODE
  #define ALARM_AUDIO_MODE 0
#endif
#ifndef ALARM_VOLUME
  #define ALARM_VOLUME 26000          // sample amplitude 0..32767
#endif
#ifndef ALARM_BUZZER_PIN
  #define ALARM_BUZZER_PIN -1
#endif

#define ALARM_SAMPLE_RATE 16000

#define ALARM_SND_OFF  0
#define ALARM_SND_HEAT 1     // overheat
#define ALARM_SND_LOST 2     // temperature signal lost
#define ALARM_SND_SLEEP 3    // cricket chirp on screen off (played ONCE, samples from RoasterCue.h)
#define ALARM_SND_WAKE  4    // cricket chirp on screen on
#define ALARM_SND_IDLE  5    // cricket chirp entering IDLE (Logo page)
#define ALARM_SND_WELCOME 6  // cricket chirp on Welcome (power on / wake)

// Control flags (sound task reads, UI writes). Single-byte flags need no lock.
static volatile uint8_t g_alarm_sound = ALARM_SND_OFF;
// Master volume 0..100 for the CHIRPS (0 = mute). Overheat / Lost alarms ignore it and always sound at ALARM_VOLUME.
// Written by the UI / RoasterUIBind.h (Setting > System > Volume, saved in NVS); read by the sound task.
static volatile uint8_t g_alarmCueVol = 60;


// Pattern: one slot per 100 ms, frequency in Hz (0 = silent)
static const uint16_t ALARM_PATTERN_HEAT[] = { 880, 880, 1320, 1320, 0, 0, 0, 0 };
static const uint16_t ALARM_PATTERN_LOST[] = { 784, 784, 784, 523, 523, 523, 0, 0, 0, 0 };
#define ALARM_PATTERN_HEAT_LEN ((int)(sizeof(ALARM_PATTERN_HEAT) / sizeof(ALARM_PATTERN_HEAT[0])))
#define ALARM_PATTERN_LOST_LEN ((int)(sizeof(ALARM_PATTERN_LOST) / sizeof(ALARM_PATTERN_LOST[0])))

// Frequency of slot idx (wrapping) for sound kind; 0 = silent or invalid kind
static uint16_t alarmFreq(uint8_t kind, int idx) {
    if (idx < 0) idx = 0;
    if (kind == ALARM_SND_HEAT) return ALARM_PATTERN_HEAT[idx % ALARM_PATTERN_HEAT_LEN];
    if (kind == ALARM_SND_LOST) return ALARM_PATTERN_LOST[idx % ALARM_PATTERN_LOST_LEN];
    return 0;
}

static_assert(CUE_SAMPLE_RATE == ALARM_SAMPLE_RATE, "RoasterCue.h and RoasterAlarmSound.h must use the same sample rate");

// One-shot sounds (chirps SLEEP / WAKE / IDLE): at the end the task sets g_alarm_sound = OFF
static inline bool alarmIsOneShot(uint8_t kind) { return kind == ALARM_SND_SLEEP || kind == ALARM_SND_WAKE || kind == ALARM_SND_IDLE || kind == ALARM_SND_WELCOME; }
static inline uint8_t alarmCueId(uint8_t kind) {
    return kind == ALARM_SND_SLEEP ? (uint8_t)CUE_SLEEP :
           kind == ALARM_SND_IDLE ? (uint8_t)CUE_IDLE :
           kind == ALARM_SND_WELCOME ? (uint8_t)CUE_WELCOME : (uint8_t)CUE_WAKE;
}
// Number of 100 ms blocks of a one-shot sound (+1 silent block so the speaker finishes the last sample)
static inline int alarmOneShotLen(uint8_t kind) { return (int)((cueParam(alarmCueId(kind)).ms + 99) / 100) + 1; }

#if (ENABLE_OVERHEAT_ALARM || ENABLE_SIGNAL_LOST_ALARM) && ALARM_AUDIO_MODE == 1
  #if __has_include("driver/i2s_std.h")
    #include <Wire.h>
    #include "driver/i2s_std.h"
    #define ALARM_HAS_I2S 1
  #endif
#endif

#if defined(ALARM_HAS_I2S)

static i2s_chan_handle_t g_alarmTx = nullptr;

// ---- ES8311 codec (playback only, slave mode, I2S 16 bit, 16 kHz, MCLK = 256 x fs = 4.096 MHz) ----
static bool alarmCodecWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission((uint8_t)ALARM_CODEC_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

static bool alarmCodecInit() {
    // Probe the codec: no ACK, do nothing more
    Wire.beginTransmission((uint8_t)ALARM_CODEC_ADDR);
    if (Wire.endTransmission() != 0) return false;

    bool ok = true;
    ok &= alarmCodecWrite(0x00, 0x1F);            // reset
    delay(20);
    ok &= alarmCodecWrite(0x00, 0x00);
    ok &= alarmCodecWrite(0x45, 0x00);
    ok &= alarmCodecWrite(0x01, 0x30);
    ok &= alarmCodecWrite(0x02, 0x00);
    ok &= alarmCodecWrite(0x03, 0x10);
    ok &= alarmCodecWrite(0x16, 0x24);
    ok &= alarmCodecWrite(0x04, 0x10);
    ok &= alarmCodecWrite(0x05, 0x00);
    ok &= alarmCodecWrite(0x0B, 0x00);
    ok &= alarmCodecWrite(0x0C, 0x00);
    ok &= alarmCodecWrite(0x10, 0x1F);
    ok &= alarmCodecWrite(0x11, 0x7F);
    ok &= alarmCodecWrite(0x00, 0x80);            // enable state machine, slave mode
    ok &= alarmCodecWrite(0x01, 0x3F);            // all clocks on, MCLK from the MCLK pin
    // Clock divider for MCLK 4.096 MHz / 16 kHz
    ok &= alarmCodecWrite(0x02, 0x00);
    ok &= alarmCodecWrite(0x03, 0x10);
    ok &= alarmCodecWrite(0x04, 0x10);
    ok &= alarmCodecWrite(0x05, 0x00);
    ok &= alarmCodecWrite(0x06, 0x03);
    ok &= alarmCodecWrite(0x07, 0x00);
    ok &= alarmCodecWrite(0x08, 0xFF);
    // I2S format 16 bit
    ok &= alarmCodecWrite(0x09, 0x0C);
    ok &= alarmCodecWrite(0x0A, 0x0C);
    // Power up analog + DAC
    ok &= alarmCodecWrite(0x17, 0xBF);
    ok &= alarmCodecWrite(0x0E, 0x02);
    ok &= alarmCodecWrite(0x12, 0x00);
    ok &= alarmCodecWrite(0x14, 0x1A);
    ok &= alarmCodecWrite(0x0D, 0x01);
    ok &= alarmCodecWrite(0x15, 0x40);
    ok &= alarmCodecWrite(0x37, 0x08);
    ok &= alarmCodecWrite(0x45, 0x00);
    ok &= alarmCodecWrite(0x31, 0x00);            // unmute DAC
    ok &= alarmCodecWrite(0x32, (uint8_t)ALARM_CODEC_VOL);   // DAC volume (0xBF = 0 dB)
    return ok;
}

static void alarm_audio_task(void *) {
    static int16_t buf[(ALARM_SAMPLE_RATE / 10) * 2];  // 100 ms stereo (L = R)
    const int N = ALARM_SAMPLE_RATE / 10;              // 1600 samples = 100 ms
    bool enabled = false;
    float phase = 0.0f;
    int pat = 0;
    uint8_t last = ALARM_SND_OFF;

    for (;;) {
        const uint8_t kind = g_alarm_sound;

        if (kind != ALARM_SND_OFF) {
            if (!enabled) {
                digitalWrite(ALARM_PA_PIN, HIGH);
                i2s_channel_enable(g_alarmTx);
                enabled = true; pat = 0; phase = 0.0f; last = kind;
            }
            if (kind != last) { pat = 0; phase = 0.0f; last = kind; }
            if (alarmIsOneShot(kind) && pat >= alarmOneShotLen(kind)) {
                if (g_alarm_sound == kind) g_alarm_sound = ALARM_SND_OFF;
                last = ALARM_SND_OFF;
                continue;
            }

            const int blk = pat;
            const uint16_t f = alarmFreq(kind, pat++);
            const int vol = (int)ALARM_VOLUME;
            if (alarmIsOneShot(kind)) {
                const uint8_t cid = alarmCueId(kind);
                for (int i = 0; i < N; i++) {
                    const int16_t v = (int16_t)(((int32_t)cueSampleAt(cid, (uint32_t)blk * (uint32_t)N + (uint32_t)i) * (int32_t)g_alarmCueVol) / 100);
                    buf[2 * i] = v;
                    buf[2 * i + 1] = v;
                }
            } else if (f == 0) {
                memset(buf, 0, sizeof(buf));
            } else {
                const float inc = 2.0f * PI * (float)f / (float)ALARM_SAMPLE_RATE;
                for (int i = 0; i < N; i++) {
                    const int16_t v = (int16_t)(sinf(phase) * vol);
                    buf[2 * i] = v;
                    buf[2 * i + 1] = v;
                    phase += inc;
                    if (phase > 2.0f * PI) phase -= 2.0f * PI;
                }
            }
            size_t written = 0;
            i2s_channel_write(g_alarmTx, buf, sizeof(buf), &written, 300);
            continue;
        }

        // Idle: disable I2S & PA to prevent speaker hiss
        if (enabled) {
            i2s_channel_disable(g_alarmTx);
            digitalWrite(ALARM_PA_PIN, LOW);
            enabled = false;
            last = ALARM_SND_OFF;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

#elif (ENABLE_OVERHEAT_ALARM || ENABLE_SIGNAL_LOST_ALARM) && ALARM_AUDIO_MODE == 2

// Chirp on a buzzer: on/off with the pulse timing at the cue frequency (a buzzer has no volume control)
static void alarmBuzzerCue(uint8_t kind) {
    const uint8_t cid = alarmCueId(kind);
    const CueParam p = cueParam(cid);
    if (g_alarmCueVol == 0) { if (g_alarm_sound == kind) g_alarm_sound = ALARM_SND_OFF; return; }   // muted
    const uint32_t t0 = millis();
    bool on = false;
    while (g_alarm_sound == kind) {
        const uint32_t el = millis() - t0;
        if (el >= p.ms) break;
        const bool gate = cueGateAt(cid, el * (uint32_t)CUE_SAMPLE_RATE / 1000UL);
        if (gate && !on)       { tone(ALARM_BUZZER_PIN, p.freq); on = true; }
        else if (!gate && on)  { noTone(ALARM_BUZZER_PIN);       on = false; }
        vTaskDelay(pdMS_TO_TICKS(3));
    }
    noTone(ALARM_BUZZER_PIN);
}

static void alarm_audio_task(void *) {
    int pat = 0;
    bool on = false;
    uint8_t last = ALARM_SND_OFF;
    for (;;) {
        const uint8_t kind = g_alarm_sound;
        if (kind == ALARM_SND_OFF || ALARM_BUZZER_PIN < 0) {
            if (on) { noTone(ALARM_BUZZER_PIN); on = false; }
            last = ALARM_SND_OFF;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (alarmIsOneShot(kind)) {                            // chirp: play once, then go silent
            alarmBuzzerCue(kind);
            if (g_alarm_sound == kind) g_alarm_sound = ALARM_SND_OFF;
            on = false; last = ALARM_SND_OFF;
            continue;
        }
        if (kind != last) { pat = 0; last = kind; }
        const uint16_t f = alarmFreq(kind, pat++);
        if (f) { tone(ALARM_BUZZER_PIN, f); on = true; }
        else   { noTone(ALARM_BUZZER_PIN);  on = false; }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

#endif

// Call once at UI start, from the UI task (see the I2C note at the top). Does nothing without a speaker / buzzer.
static void alarmSoundStart() {
#if defined(ALARM_HAS_I2S)
    pinMode(ALARM_PA_PIN, OUTPUT);
    digitalWrite(ALARM_PA_PIN, LOW);
    if (!alarmCodecInit()) {
        DLOGM("ALARM", "codec ES8311 khong tra loi (I2C 0x%02X): tat am thanh canh bao\n", (unsigned)ALARM_CODEC_ADDR);
        return;
    }
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    if (i2s_new_channel(&chan_cfg, &g_alarmTx, NULL) != ESP_OK) {
        DLOGM("ALARM", "i2s_new_channel loi\n");
        return;
    }
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(ALARM_SAMPLE_RATE),                       // MCLK = 256 x fs
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (gpio_num_t)ALARM_I2S_MCLK,
            .bclk = (gpio_num_t)ALARM_I2S_BCLK,
            .ws   = (gpio_num_t)ALARM_I2S_LRCK,
            .dout = (gpio_num_t)ALARM_I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    if (i2s_channel_init_std_mode(g_alarmTx, &std_cfg) != ESP_OK) {
        DLOGM("ALARM", "i2s_channel_init_std_mode loi\n");
        i2s_del_channel(g_alarmTx);
        g_alarmTx = nullptr;
        return;
    }
    DLOGM("ALARM", "codec ES8311 san sang (I2S MCLK=%d BCLK=%d LRCK=%d DOUT=%d, PA=%d)\n",
          (int)ALARM_I2S_MCLK, (int)ALARM_I2S_BCLK, (int)ALARM_I2S_LRCK, (int)ALARM_I2S_DOUT, (int)ALARM_PA_PIN);
    xTaskCreatePinnedToCore(alarm_audio_task, "alarm", 4096, NULL, 1, NULL, 0);
#elif (ENABLE_OVERHEAT_ALARM || ENABLE_SIGNAL_LOST_ALARM) && ALARM_AUDIO_MODE == 2
    xTaskCreatePinnedToCore(alarm_audio_task, "alarm", 4096, NULL, 1, NULL, 0);
#endif
}
