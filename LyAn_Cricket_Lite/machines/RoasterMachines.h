/*
 * machines/RoasterMachines.h - Roaster selector (the "socket" for each machine driver)
 *
 * Each roaster is ONE folder: machines/<Brand>_<Model>/, every machine uses the same file names:
 *   RoasterMachineInfo.h  Machine profile: type, fuel, power, batch size, role of Air, capabilities
 *   RoasterMachine.h      Socket: receives temperature, sends the control frame: machinePoll / machineSend / machineSetTemp
 *   RoasterParser.h       Decodes the machine's signal
 *   RoasterDialect.h      HiBean dialect for this machine family: incoming commands / READ reply
 *
 * Adding a new machine (e.g. Kaleido M10):
 *   1. Create machines/Kaleido_M10/ with all the files above.
 *   2. RoasterSetting.h: add  #define ENABLE_MACHINE_KALEIDO_M10 0  (set 1 and disable the other to use it).
 *   3. This file: add one #elif branch for the new machine and one line in the count below.
 *
 * Tier 4 (Dialect calls the tier-3 control core): may include lower tiers only. No UI or network includes.
 */
#pragma once
#include "../RoasterSetting.h"

// ---- Standard values used in machine profiles (RoasterMachineInfo.h) ----
// Machine type
#define MACHINE_TYPE_HOT_AIR   1   // hot air: a hot air stream heats and lifts the beans
#define MACHINE_TYPE_DRUM      2   // rotating drum
#define MACHINE_TYPE_HYBRID    3   // rotating drum + hot air
// Fuel
#define MACHINE_FUEL_ELECTRIC  1
#define MACHINE_FUEL_GAS       2
// Role of Air (sets the minimum Air level, the Air factor in simulation, the display label)
#define MACHINE_AIR_HOT_AIR_FLOW 1 // Air = hot air flow: carries heat and lifts beans; low Air with Heater on overheats the element and strongly affects BT
#define MACHINE_AIR_EXHAUST      2 // Air = exhaust / chimney damper: affects BT indirectly

// ---- Count enabled machines: must be exactly ONE ----
#define MACHINES_ENABLED_COUNT ( (ENABLE_MACHINE_SKYWALKER_V1 + 0) )

#if MACHINES_ENABLED_COUNT != 1
  #error "Phai bat DUNG MOT may rang: dat dung mot co ENABLE_MACHINE_... = 1 o muc 3 cua RoasterSetting.h"
#endif

// ---- Load the selected machine profile ----
#if (ENABLE_MACHINE_SKYWALKER_V1 + 0) == 1
  #include "Skywalker/RoasterMachineInfo.h"
#endif

// ---- The machine profile MUST define these macros ----
#ifndef MACHINE_ID
  #error "RoasterMachineInfo.h thieu MACHINE_ID"
#endif
#ifndef MACHINE_NAME
  #error "RoasterMachineInfo.h thieu MACHINE_NAME"
#endif
#ifndef MACHINE_TYPE
  #error "RoasterMachineInfo.h thieu MACHINE_TYPE"
#endif
#ifndef MACHINE_FUEL
  #error "RoasterMachineInfo.h thieu MACHINE_FUEL"
#endif
#ifndef MACHINE_POWER_W
  #error "RoasterMachineInfo.h thieu MACHINE_POWER_W"
#endif
#ifndef MACHINE_BATCH_STD_G
  #error "RoasterMachineInfo.h thieu MACHINE_BATCH_STD_G"
#endif
#ifndef MACHINE_AIR_ROLE
  #error "RoasterMachineInfo.h thieu MACHINE_AIR_ROLE"
#endif

// ---- Shared description strings (Serial banner, About page, web pages) ----
#if MACHINE_TYPE == MACHINE_TYPE_HOT_AIR
  #define MACHINE_TYPE_TXT "hot air"
#elif MACHINE_TYPE == MACHINE_TYPE_DRUM
  #define MACHINE_TYPE_TXT "drum"
#else
  #define MACHINE_TYPE_TXT "hybrid"
#endif
#if MACHINE_FUEL == MACHINE_FUEL_ELECTRIC
  #define MACHINE_FUEL_TXT "electric"
#else
  #define MACHINE_FUEL_TXT "gas"
#endif
#if MACHINE_AIR_ROLE == MACHINE_AIR_HOT_AIR_FLOW
  #define MACHINE_AIR_ROLE_TXT "hot-air flow"
#else
  #define MACHINE_AIR_ROLE_TXT "exhaust"
#endif

// ---- Load the selected machine driver (after the profile is checked) ----
#if (ENABLE_MACHINE_SKYWALKER_V1 + 0) == 1
  #include "Skywalker/RoasterMachine.h"       // pulls in RoasterParser.h
  #include "Skywalker/RoasterDialect.h"       // pulls in lib/RoasterControl.h
#endif
