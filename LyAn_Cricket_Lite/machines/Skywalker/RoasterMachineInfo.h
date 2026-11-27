/*
 * machines/Skywalker/RoasterMachineInfo.h - Machine profile: SKYWALKER V1
 *
 * Declarations only. Figures come from project notes (Skywalker V1 log, 1000 W) and the real roasted batch size (250-400 g).
 * Edit the numbers here if the machine differs; simulation, About and web pages read them from here.
 * Standard values (MACHINE_TYPE_*, ...) are defined in machines/RoasterMachines.h.
 */
#pragma once

#define MACHINE_ID            "skywalker_v1"     // short id: used in file names, logs, profiles
#define MACHINE_BRAND         "Skywalker"
#define MACHINE_MODEL         "V1"
#define MACHINE_NAME          "Skywalker V1"

// ---- Description ----
#define MACHINE_TYPE          MACHINE_TYPE_HOT_AIR     // hot air
#define MACHINE_FUEL          MACHINE_FUEL_ELECTRIC    // electric
#define MACHINE_POWER_W       1000                     // max Heater power (W); for gas, the thermal power
#define MACHINE_BATCH_MIN_G   250                      // smallest real batch (g)
#define MACHINE_BATCH_STD_G   400                      // standard batch (g): simulation and the 19 sample profiles are tuned to this batch
#define MACHINE_BATCH_MAX_G   400                      // largest real batch (g)
#define MACHINE_AIR_ROLE      MACHINE_AIR_HOT_AIR_FLOW // Air is the hot air flow

// ---- Capabilities (UI and core use these to hide / show cards and ignore missing commands) ----
#define MACHINE_HAS_HEATER    1
#define MACHINE_HAS_AIR       1
#define MACHINE_HAS_DRUM      1                        // has a rotating drum (on / off)
#define MACHINE_HAS_COOL      1                        // has a cooling fan (on / off)
#define MACHINE_HAS_ET        0                        // no separate ET probe (HiBean receives BT twice)
#define MACHINE_HEATER_STEP   5                        // Heater step (%)
#define MACHINE_AIR_STEP      5                        // Air step (%)

// ---- Display labels (unaccented, used on screen) ----
#define MACHINE_LABEL_HEATER  "Heater"
#define MACHINE_LABEL_AIR     "Air Fan"
