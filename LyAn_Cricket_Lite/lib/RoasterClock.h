/*
 * lib/RoasterClock.h - Time module: NTP + browser sync (AP/offline) + automatic timezone (tier 1)
 *
 * Includes tier 0 only (RoasterSetting.h, RoasterSync.h) and standard ESP-IDF/Arduino libraries.
 *   - The system clock always runs UTC.
 *   - Local timezone (minutes from UTC, e.g. +420 = UTC+7, -300 = UTC-5, +330 = UTC+5:30) is stored in NVS ("lygia" / "tz_min").
 *   - Web Portal (AP offline or STA): the browser POSTs /api/time with the UTC epoch and its current offset
 *     (-getTimezoneOffset(), DST-correct); the device updates and saves it (NVS written only on change).
 *   - Thread-safe on both cores: clockLocalTm() uses gmtime_r(now + tzMin*60), independent of the TZ environment variable,
 *     while the POSIX TZ is kept in sync for other standard calls.
 *   - Call clockInit() once in setup(); clockStartNtp() when STA has an IP; clockSyncFromBrowser() from /api/time.
 */
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"

#define HAS_MODULE_CLOCK 1

// Earliest valid timestamp: 2024-01-01 00:00:00 UTC (avoids year 1970)
#ifndef CLOCK_MIN_VALID
  #define CLOCK_MIN_VALID       1704067200LL
#endif
#ifndef CLOCK_MAX_VALID
  #define CLOCK_MAX_VALID       4102444800LL    // 01/01/2100 UTC
#endif
#ifndef CLOCK_DEFAULT_TZ_MIN
  #define CLOCK_DEFAULT_TZ_MIN  420             // Default UTC+7
#endif
#ifndef CLOCK_BOOT_WAIT_MS
  #define CLOCK_BOOT_WAIT_MS    30000UL         // Max wait for NTP after WiFi connects at boot
#endif
#ifndef CLOCK_BOOT_TOTAL_MS
  #define CLOCK_BOOT_TOTAL_MS   45000UL         // Max total wait from boot if WiFi is not up yet
#endif

static volatile int16_t  g_clockTzMin      = CLOCK_DEFAULT_TZ_MIN;
static volatile bool     g_clockNtpSynced  = false;
static bool              g_clockNtpStarted = false;
static bool              g_clockInited     = false;
static char              g_clockPosixTz[24] = "UTC-7:00";

// Convert UTC offset in minutes to a POSIX TZ string (POSIX inverts the sign: UTC+7 -> "UTC-7:00")
static void clockBuildPosixTz(int16_t offsetMin, char *out, size_t cap) {
    if (offsetMin < -720 || offsetMin > 840) offsetMin = CLOCK_DEFAULT_TZ_MIN;
    const char sign = (offsetMin >= 0) ? '-' : '+';
    const int absMin = (offsetMin >= 0) ? (int)offsetMin : -(int)offsetMin;
    snprintf(out, cap, "UTC%c%d:%02d", sign, absMin / 60, absMin % 60);
}

static void clockApplyPosixTz() {
    clockBuildPosixTz((int16_t)g_clockTzMin, g_clockPosixTz, sizeof g_clockPosixTz);
    setenv("TZ", g_clockPosixTz, 1);
    tzset();
}

static void clockNtpSyncCb(struct timeval *tv) {
    (void)tv;
    g_clockNtpSynced = true;
}

// 1. Init once in setup(): load the saved timezone from NVS, attach the SNTP callback
static void clockInit() {
    if (g_clockInited) return;
    g_clockInited = true;
    Preferences p;
    if (p.begin("lygia", true)) {
        const int16_t saved = p.getShort("tz_min", CLOCK_DEFAULT_TZ_MIN);
        if (saved >= -720 && saved <= 840) g_clockTzMin = saved;
        p.end();
    }
    clockApplyPosixTz();
    sntp_set_time_sync_notification_cb(clockNtpSyncCb);
}

// 2. Set timezone (minutes from UTC); NVS is written only on change to limit flash wear
static void clockSetTzOffsetMin(int16_t offsetMin) {
    if (offsetMin < -720 || offsetMin > 840) return;
    if (offsetMin == (int16_t)g_clockTzMin) return;
    g_clockTzMin = offsetMin;
    clockApplyPosixTz();
    Preferences p;
    if (p.begin("lygia", false)) {
        p.putShort("tz_min", offsetMin);
        p.end();
    }
    DLOGM("CLOCK", "cap nhat mui gio: %+d phut (%s)\n", (int)offsetMin, g_clockPosixTz);
}

// 3. Start NTP sync (once, when STA has an IP). Does not restart SNTP if running.
static void clockStartNtp() {
#if ENABLE_WIFI
    if (g_clockNtpStarted) return;
    g_clockNtpStarted = true;
    configTzTime(g_clockPosixTz, NTP_SERVER1, NTP_SERVER2);
#endif
}

// Call when WiFi is fully off: if NTP never synced, let the next WiFi connect run SNTP again
static void clockOnWifiDown() {
    if (!g_clockNtpSynced) g_clockNtpStarted = false;
}

// 4. Sync time and timezone from the Web Portal browser (works in AP offline and STA)
static bool clockSyncFromBrowser(time_t utcEpoch, int16_t tzOffsetMin) {
    clockSetTzOffsetMin(tzOffsetMin);
    if ((int64_t)utcEpoch < CLOCK_MIN_VALID || (int64_t)utcEpoch > CLOCK_MAX_VALID) return false;
    // If NTP is already synced, keep NTP time and take only the timezone
    if (g_clockNtpSynced) return true;
    struct timeval tv;
    tv.tv_sec  = utcEpoch;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    return true;
}

// 5. Read clock state (thread-safe on both cores)
static inline bool    clockIsValid()     { return (int64_t)time(nullptr) >= CLOCK_MIN_VALID; }

// Local time, thread-safe (independent of the global TZ variable)
static bool clockLocalTm(struct tm &out) {
    const time_t now = time(nullptr);
    if ((int64_t)now < CLOCK_MIN_VALID) return false;
    const time_t localSec = now + (time_t)((int32_t)g_clockTzMin * 60);
    gmtime_r(&localSec, &out);
    return true;
}

// Short timezone label: "UTC+7", "UTC-5", "UTC+5:30"
static void clockTzLabel(char *out, size_t cap) {
    const int m = (int)g_clockTzMin;
    const char sign = (m >= 0) ? '+' : '-';
    const int am = (m >= 0) ? m : -m;
    if (am % 60 == 0) snprintf(out, cap, "UTC%c%d", sign, am / 60);
    else              snprintf(out, cap, "UTC%c%d:%02d", sign, am / 60, am % 60);
}

// "HH:MM" for the 1.54" screen (true if the time is valid)
static bool clockFormatHHMM(char *buf, size_t cap) {
    struct tm t{};
    if (!clockLocalTm(t)) {
        if (cap > 0) buf[0] = '\0';
        return false;
    }
    snprintf(buf, cap, "%02d:%02d", t.tm_hour, t.tm_min);
    return true;
}

// Full string for the web home page: "19/10/2026 - 08:41 (UTC+7)"
static String clockStrFull() {
    struct tm t{};
    if (!clockLocalTm(t)) return String("--");
    char b[32], tz[16];
    strftime(b, sizeof b, "%d/%m/%Y - %H:%M", &t);
    clockTzLabel(tz, sizeof tz);
    return String(b) + " (" + String(tz) + ")";
}

// Local ISO string for the roast log: "2026-10-19T08:41:05" (empty if no time)
static String clockIsoTime() {
    struct tm t{};
    if (!clockLocalTm(t)) return String("");
    char b[32];
    strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%S", &t);
    return String(b);
}
