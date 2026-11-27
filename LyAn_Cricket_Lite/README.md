# LyAn Cricket Lite — Open-Source Community Edition (GPLv3)

Compact, standalone ESP32-S3 controller firmware for the **Skywalker V1** coffee roaster, built for the **Waveshare ESP32-S3-Touch-LCD-1.54** (240×240 capacitive touch + 3 tactile buttons + I2S audio).

---

## Features

* **Standalone Manual Roasting + Standard PID:** Direct control of Heater, Air Fan, Drum, and Cool Fan, plus standard PID (`PID_v1` 1.2.0) with editable `Kp / Ki / Kd / P-Mode` saved to NVS and one-tap **Default** restore.
* **Live Telemetry & Roast Events:** Real-time Bean Temp (BT), OLS linear regression RoR (HiBean-compatible), Development Time (DEV / +ΔT° / %DTR), Turning Point (TP) detection, and 6 event markers (`PR`, `CH`, `DE`, `1FC`, `2FC`, `DR`).
* **Multi-Mode Connectivity:**
  * **Standalone** (offline roasting)
  * **HiBean BLE** (wireless Bluetooth LE bridge)
  * **Artisan WebSocket** (port `81`, with `.aset` config download)
  * **Web Portal** (port `80`, `http://lycricket.local` or SoftAP `AP_Cricket` at `http://192.168.4.1`, Wi-Fi setup, Artisan config, and About page)
* **Tactile Cockpit UI (LVGL 9.2.x):** 3 languages (**EN / VI / CN**), °C/°F unit switch, Welcome screen with synthesized cricket cue, IDLE logo screen, and hardware button shortcuts.
* **Built-in Simulator (`SIM`):** Automatically enters simulation mode when no physical roaster is connected so you can test UI, PID, HiBean, and Artisan safely at your desk.
* **Safety Interlocks & Alarms:** Overheat heater cutoff (default `240 °C`, configurable `220–260 °C`), temp signal-lost cutoff (`> 5 s`), automatic Drum lock when `BT >= 50 °C`, minimum `10%` Air while heating, 5-second confirm popups for `DROP` and `END`, and I2S/buzzer audio alarms.

---

## Community Edition (GPLv3) vs. LyAn Pro Edition

| Feature | Community Edition (This Repo — GPLv3) | LyAn Pro Edition (Commercial) |
| :--- | :---: | :---: |
| Skywalker V1 core, SIM mode, Safety interlocks | Yes | Yes |
| Manual Roast + Standard PID (`Kp/Ki/Kd/P-Mode`) | Yes | Yes |
| HiBean BLE + Artisan WebSocket + Web Portal (Wi-Fi / `.aset`) | Yes | Yes |
| LVGL 1.54" UI (EN / VI / CN, Welcome cue, IDLE logo) | Yes | Yes |
| PID Assist, Predictive Braking & Tuned Presets (`Soft/Balanced/Fast/P1–P3`) | — | Yes |
| Auto-Roast by Profile & Web Profile Editor | — | Yes |
| Dual-Storage Roast Logging (`FFat ↔ microSD` sync) | — | Yes |
| Multi-language Voice Temp Announcements (`.pak` + PSRAM) | — | Yes |
| Local & Cloud OTA Firmware Updates | — | Yes |

For commercial inquiries or the **LyAn Pro Edition**, contact **LyAn Roaster LAB** at **lycoffee.ai@gmail.com**.

---

## Hardware & Pinout

* **Board:** Waveshare ESP32-S3-Touch-LCD-1.54 (16 MB Flash, 8 MB OPI PSRAM)
* **Display (ST7789 SPI):** `DC 45`, `CS 21`, `SCK 38`, `MOSI 39`, `RST 40`, `BL 46`
* **Touch (CST816S I2C `0x15`):** `SDA 42`, `SCL 41`, `RST 47`, `INT 48`
* **Buttons (active LOW):** `BOOT GPIO0` (`-`), `PLUS GPIO4` (`+` / long-press `0.8s` to cycle link mode), `PWR GPIO5` (`Done` / screen sleep / long-press `5s` for Power menu)
* **Audio (ES8311 + NS4150B I2S):** Defined in `boards/Board_Waveshare_ESP32S3_Touch_LCD_154.h`
* **Battery:** `ADC GPIO1`, `HOLD GPIO2`, `CHG GPIO3`
* **Skywalker V1 Interface:** `TX GPIO19`, `RX GPIO20` *(set `SERIAL_DEBUG 0` in `RoasterSetting.h` when connected to a real roaster)*

---

## Build Instructions (Arduino IDE)

1. **Board Package:** Install `esp32` by Espressif Systems (3.x).
2. **Board Settings:**
   * Board: `ESP32S3 Dev Module`
   * Flash Size: `16MB (128Mb)`
   * PSRAM: `OPI PSRAM`
   * Partition Scheme: uses `partitions.csv` in the sketch folder
   * USB CDC On Boot: `Enabled` (only when `SERIAL_DEBUG 1`)
3. **Required Libraries:**
   * `PID` (by Brett Beauregard) `1.2.0`
   * `NimBLE-Arduino`
   * `WebSockets` (by Markus Sattler / Links2004)
   * `GFX Library for Arduino` (Moon On Our Nation)
   * `lvgl` `9.2.x`
4. **Required `lv_conf.h` Setting (in your Arduino `libraries/` folder):**
   ```c
   #define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
   ```
   *(Do not use `LV_STDLIB_BUILTIN`, as its static internal-RAM pool reduces free heap for Wi-Fi/BLE.)*
5. **Default Web Portal Credentials** (editable in `RoasterSetting.h`):
   * SoftAP SSID / Pass: `AP_Cricket` / `12345678`
   * Web Portal PIN: `1234` (set by `WEB_PIN` in `RoasterSetting.h`; compile-time only, it cannot be changed from the device screen)

---

## Architecture & Development

* **Dual-Core Split:**
  * **Core 1 (`loop()`):** Roaster protocol (`500 ms` cycle), PID, safety watchdog, simulator, BLE service.
  * **Core 0 (`ui_task` `100 ms`):** LVGL 9.2.x UI, Wi-Fi, Web Portal, Artisan WebSocket, battery monitor.
* **Project Layout:**
  ```text
  LyAn_Cricket_Lite.ino      Entry point (setup / loop)
  RoasterSetting.h           All compile-time options, defaults, safety limits
  partitions.csv             16 MB flash partition table
  boards/                    Board pin maps (Waveshare ESP32-S3-Touch-LCD-1.54)
  machines/                  Roaster drivers (Skywalker V1: protocol, parser, HiBean dialect)
  lib/                       Core modules (control, PID, batch/RoR, sim, net, web, BLE, Artisan, UI)
  tools/check_includes.py    Include-tier checker
  ```
* **Persistent Settings (NVS namespace `lygia`):** temperature unit (`unit`), overheat cutoff (`warn`), simulator speed (`simx`), PID `pkp / pki / pkd / ppm`, UI language / idle time / brightness / volume (`ui_lang`, `idlem`, `bl_pct`, `vol_pct`, `snd_mute`), saved Wi-Fi networks and link mode.
* **Strict One-Way Include Hierarchy:** Run `python tools/check_includes.py` after adding or modifying any `#include` directive to verify zero tier violations.

---

## License

Copyright (C) 2026 **LyAn Roaster LAB** (`lycoffee.ai@gmail.com`).  
This project is licensed under the **GNU General Public License v3.0 or later (GPL-3.0-or-later)** — see [LICENSE](LICENSE) for details.

---

## Maintenance Notes

* The Community Edition keeps no code for Pro-only features (profiles, roast logs, OTA, file manager, storage sync); related web-portal strings and unused UI icon assets were removed.
* Generated asset headers (`lib/UIConfig.h` fonts, `lib/RoasterUIIdle.h`, `lib/RoasterUIWidgets.h`, `lib/RoasterIcons_std.h`, `lib/RoasterWebLogo.h`) contain large bitmap/font tables; edit them with their generator tools, not by hand.
* Run `python tools/check_includes.py` after every `#include` change (see [CONTRIBUTING.md](CONTRIBUTING.md)).

