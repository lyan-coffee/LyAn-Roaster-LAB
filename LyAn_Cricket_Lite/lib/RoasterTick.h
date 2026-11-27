/*
 * lib/RoasterTick.h - 100 ms tick list for modules running on core 0 (tier 3)
 *
 * Modules register functions with tickAdd(); one list runs them on core 0:
 *   tickAdd(fn)      register a function to run every TICK_MS (default 100 ms), in registration order
 *   tickRun()        run the whole list once
 *   tickStartTask()  create a core-0 task calling tickRun() every TICK_MS (use when there is no UI)
 *
 * With the UI module, the UI sync timer calls tickRun() instead and the .ino does not call tickStartTask().
 * Listed functions must not block for long.
 * Tier 3: includes tier 0 only.
 */
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"

#define HAS_MODULE_TICK 1

typedef void (*TickFn)();
#define TICK_MAX 8
static TickFn   g_tickFns[TICK_MAX];
static uint8_t  g_tickN = 0;
static bool     g_tickTaskOn = false;
static volatile bool g_tickPause = false;      // true = tickRun() does nothing

static bool tickAdd(TickFn fn) {
    if (!fn) return false;
    for (uint8_t i = 0; i < g_tickN; i++) if (g_tickFns[i] == fn) return false;     // already registered
    if (g_tickN >= TICK_MAX) return false;
    g_tickFns[g_tickN++] = fn;
    return true;
}

// Run the whole list once, ignoring g_tickPause
static void tickRunAll() {
    for (uint8_t i = 0; i < g_tickN; i++) g_tickFns[i]();
}

// Called by the tick task (or the UI timer)
static void tickRun() {
    if (!g_tickPause) tickRunAll();
}

static void tickTaskFn(void *) {
    for (;;) {
        tickRun();
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

// Create the core-0 tick task (once). Use when there is no UI.
static bool tickStartTask() {
    if (g_tickTaskOn) return true;
    if (xTaskCreatePinnedToCore(tickTaskFn, "tick", TICK_TASK_STACK, nullptr, 1, nullptr, 0) != pdPASS) return false;
    g_tickTaskOn = true;
    return true;
}
