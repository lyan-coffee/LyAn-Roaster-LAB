/*
 * lib/RoasterGraphData.h - Roast Telemetry Buffer & Turning Point (TP) Detection (Tier 3)
 *
 * Samples Bean Temp (BT) and RoR once per second from Charge (up to GRAPH_MAX_S).
 * Automatically detects the Turning Point (TP) once BT rises >= 1.0 C above its minimum.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <Arduino.h>
#include "../RoasterSetting.h"
#include "RoasterSync.h"
#include "RoasterState.h"
#include "RoasterBatch.h"

#if ENABLE_ROAST_GRAPH

#define HAS_MODULE_GRAPH_DATA 1

#define GRAPH_MAX_S      1200         // Max roast duration buffered (20 min)
#define GRAPH_NO_ROR     ((int16_t)-32768)

static int16_t  gBT[GRAPH_MAX_S];     // Bean temp x10 (C)
static int16_t  gROR[GRAPH_MAX_S];    // RoR x10 (C/min) or GRAPH_NO_ROR
static volatile uint16_t gCount = 0;  // Valid sample count (index = seconds since Charge)
static volatile int gTpIdx = -1;      // Turning point index (s), -1 = not yet confirmed
static uint16_t gMinIdx = 0;          // Minimum BT sample index so far

static void graphReset() { gCount = 0; gTpIdx = -1; gMinIdx = 0; }

// Call from loop() on Core 1.
static void graphUpdate() {
    LOCK();
    if (!batchCharged) {
        if (gCount != 0) graphReset();
    } else if (batchRunning) {
        const int idx = (int)(batchElapsedMs() / 1000);
        while ((int)gCount <= idx && gCount < GRAPH_MAX_S) {
            const int i = gCount;
            const float t = constrain((float)temp, -50.0f, 300.0f);
            gBT[i]  = (int16_t)lroundf(t * 10.0f);
            gROR[i] = rorOk ? (int16_t)lroundf(constrain(rorValue, -300.0f, 300.0f) * 10.0f) : GRAPH_NO_ROR;
            if (i == 0 || gBT[i] < gBT[gMinIdx]) gMinIdx = (uint16_t)i;
            gCount = i + 1;
        }
        if (gTpIdx < 0 && gCount > 1 && gBT[gCount - 1] >= gBT[gMinIdx] + 10) gTpIdx = gMinIdx;
    }
    UNLOCK();
}

#endif  // ENABLE_ROAST_GRAPH
