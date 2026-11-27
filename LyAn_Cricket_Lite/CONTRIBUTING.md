# Contributing to LyAn Cricket Lite (OSS)

Contributions (bug fixes, new roaster machine drivers, board definitions, translations, and UI improvements) are welcome!

## Engineering Rules

1. **One-Way Include Hierarchy:**
   * Never include a higher-tier header from a lower-tier header.
   * After adding a new file or changing any `#include`, register its tier in `tools/check_includes.py` and verify:
     ```bash
     python tools/check_includes.py
     # Expected: => OK, khong vi pham
     ```
2. **Dual-Core Thread Safety (ESP32-S3):**
   * **Core 1 (`loop()`):** Real-time roaster I/O, PID, safety interlocks, BLE. Never call LVGL functions from Core 1.
   * **Core 0 (`ui_task`):** LVGL UI, Wi-Fi, HTTP Web Portal, Artisan WebSocket. Always read roaster state through `roasterSnapshot()` and protect shared state with `LOCK()` / `UNLOCK()`.
3. **LVGL UI Guidelines:**
   * Never destroy or rebuild screens directly inside a touch event callback; set `g_uiRebuildReq = true` so `uiRebuildNow()` executes safely on the next `100 ms` UI tick.
   * Keep comments concise and in English.
4. **License:**
   * All contributions are submitted under the **GPL-3.0-or-later** license.
