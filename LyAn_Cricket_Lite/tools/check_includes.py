#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
tools/check_includes.py - One-way Include Tier Checker for LyAn_Cricket_Lite (OSS)

Rule: a file at tier T may only include local headers at tier <= T.
Run:  python tools/check_includes.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TIER_RULES = {
    'RoasterSetting.h': 0,
    'boards/Board_Waveshare_ESP32S3_Touch_LCD_154.h': 0,
    'boards/RoasterBoard.h': 0,
    'lib/RoasterAsetData.h': 0,
    'lib/RoasterIcons_std.h': 0,
    'lib/RoasterSync.h': 0,
    'lib/UIConfig.h': 0,

    'lib/RoasterBattery.h': 1,
    'lib/RoasterButtons.h': 1,
    'lib/RoasterClock.h': 1,
    'lib/RoasterCue.h': 1,
    'lib/RoasterDisplay.h': 1,
    'lib/RoasterIcons.h': 1,
    'lib/RoasterState.h': 1,
    'lib/RoasterTouch.h': 1,
    'lib/RoasterUIBridge.h': 1,
    'lib/RoasterUIIdle.h': 1,
    'lib/RoasterUINav.h': 1,
    'lib/UILang.h': 1,

    'lib/RoasterAlarmSound.h': 2,
    'lib/RoasterBatch.h': 2,
    'lib/RoasterPID.h': 2,

    'lib/RoasterControl.h': 3,
    'lib/RoasterGraphData.h': 3,
    'lib/RoasterRadio.h': 3,
    'lib/RoasterSim.h': 3,
    'lib/RoasterTick.h': 3,
    'machines/Skywalker/RoasterMachine.h': 3,
    'machines/Skywalker/RoasterMachineInfo.h': 3,
    'machines/Skywalker/RoasterParser.h': 3,

    'lib/RoasterNet.h': 4,
    'lib/RoasterSettings.h': 4,
    'lib/RoasterWebLogo.h': 4,
    'machines/RoasterMachines.h': 4,
    'machines/Skywalker/RoasterDialect.h': 4,

    'lib/RoasterArtisan.h': 5,
    'lib/RoasterBLE.h': 5,
    'lib/RoasterWeb.h': 5,
    'lib/RoasterWebI18n.h': 5,

    'lib/RoasterUIWidgets.h': 6,
    'lib/RoasterWebArtisan.h': 6,

    'lib/RoasterUIAlert.h': 7,
    'lib/RoasterUIBind.h': 7,
    'lib/RoasterUIControl.h': 7,
    'lib/RoasterUIEvent.h': 7,
    'lib/RoasterUIMain.h': 7,
    'lib/RoasterUIPower.h': 7,
    'lib/RoasterUIWelcome.h': 7,

    'lib/RoasterUI.h': 8,
    'LyAn_Cricket_Lite.ino': 9,
}

INC_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')

def main():
    files = []
    for dp, dn, fn in os.walk(ROOT):
        dn[:] = [d for d in dn if d not in ('.git', 'build', '__pycache__', 'tools')]
        for f in fn:
            if f.endswith(('.ino', '.h', '.cpp')):
                rel = os.path.relpath(os.path.join(dp, f), ROOT).replace('\\', '/')
                files.append(rel)
    files.sort()

    errors = []
    print(f"{'file':52s} tier")
    for rel in files:
        if rel not in TIER_RULES:
            errors.append(f"UNDECLARED TIER: {rel}")
            continue
        t = TIER_RULES[rel]
        print(f"{rel:52s} {t}")
        full = os.path.join(ROOT, rel)
        base_dir = os.path.dirname(full)
        with open(full, 'r', encoding='utf-8', errors='replace') as fh:
            for lineno, line in enumerate(fh, 1):
                m = INC_RE.match(line)
                if not m:
                    continue
                inc_name = m.group(1)
                if inc_name.startswith(('driver/', 'esp_', 'freertos/', 'soc/', 'hal/')):
                    continue
                target_abs = os.path.normpath(os.path.join(base_dir, inc_name))
                target_rel = os.path.relpath(target_abs, ROOT).replace('\\', '/')
                if not os.path.exists(target_abs):
                    errors.append(f"MISSING INCLUDE: {rel}:{lineno} -> {target_rel}")
                    continue
                if target_rel not in TIER_RULES:
                    errors.append(f"UNDECLARED TARGET TIER: {rel}:{lineno} -> {target_rel}")
                    continue
                tt = TIER_RULES[target_rel]
                if tt > t:
                    errors.append(f"TIER VIOLATION: {rel} (tier {t}) includes {target_rel} (tier {tt}) at line {lineno}")

    print()
    if errors:
        for e in errors:
            print("ERROR:", e)
        sys.exit(1)
    print("=> OK, khong vi pham")

if __name__ == '__main__':
    main()
