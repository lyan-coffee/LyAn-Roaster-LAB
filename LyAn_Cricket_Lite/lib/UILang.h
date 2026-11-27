/*
 * lib/UILang.h - Multi-language UI String Table (VI / EN / CN) (Tier 1)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <Preferences.h>
#include "UIConfig.h"

enum UiLang : uint8_t {
    UILANG_VI = 0,
    UILANG_EN = 1,
    UILANG_CN = 2,
    UILANG_COUNT = 3
};

enum UiStrId : uint8_t {
    STR_START = 0,
    STR_MODE,
    STR_SETTING,
    STR_IDLE,
    STR_MANUAL,
    STR_ROAST,
    STR_BACK,
    STR_HEATER,
    STR_AIR,
    STR_DRUM,
    STR_COOL,
    STR_END,
    STR_DONE,
    STR_OFF,
    STR_CONFIRM,
    STR_SURE,
    STR_WIFI,
    STR_WEB_PORTAL,
    STR_LANGUAGE,
    STR_SAVE,
    STR_CANCEL,
    STR_DEFAULT,
    STR_RESET,
    STR_POWER_OFF,
    STR_STANDALONE,
    STR_CONNECTED,
    STR_WAIT_CONN,
    STR_OVERHEAT,
    STR_SIGNAL_LOST,
    STR_AUTO_PROTECT,
    STR_CHECK_CABLE,
    STR_DISMISS,
    STR_HOME,
    STR_IP_ADDRESS,
    STR_CONFIRM_END,
    STR_CONFIRM_DR,
    STR_SYSTEM,
    STR_IDLE_TIME,
    STR_BRIGHTNESS,
    STR_HEATER_OFF,
    STR_TEMP_UNIT,
    STR_T_MANUAL,
    STR_T_SETTING,
    STR_T_IDLE,
    STR_VOLUME,
    STR_COUNT
};

static UiLang g_uiLang = static_cast<UiLang>(UI_LANG_DEFAULT);

static inline bool uiLangEnabled(UiLang l) {
    if (l == UILANG_VI) return UI_LANG_VI != 0;
    if (l == UILANG_EN) return UI_LANG_EN != 0;
    if (l == UILANG_CN) return UI_LANG_CN != 0;
    return false;
}

static inline UiLang uiLangFirstEnabled() {
    if (uiLangEnabled(UILANG_VI)) return UILANG_VI;
    if (uiLangEnabled(UILANG_EN)) return UILANG_EN;
    if (uiLangEnabled(UILANG_CN)) return UILANG_CN;
    return UILANG_EN;
}

static inline void uiLangInit() {
    Preferences prefs;
    if (prefs.begin(UI_NVS_NS, true)) {
        uint8_t saved = prefs.getUChar(UI_NVS_KEY_LANG, UI_LANG_DEFAULT);
        prefs.end();
        if (saved < UILANG_COUNT && uiLangEnabled(static_cast<UiLang>(saved))) {
            g_uiLang = static_cast<UiLang>(saved);
            return;
        }
    }
    g_uiLang = uiLangEnabled(static_cast<UiLang>(UI_LANG_DEFAULT))
                   ? static_cast<UiLang>(UI_LANG_DEFAULT)
                   : uiLangFirstEnabled();
}

static inline void uiLangSet(UiLang l) {
    if (!uiLangEnabled(l) || l == g_uiLang) return;
    g_uiLang = l;
    Preferences prefs;
    if (prefs.begin(UI_NVS_NS, false)) {
        prefs.putUChar(UI_NVS_KEY_LANG, static_cast<uint8_t>(l));
        prefs.end();
    }
}

static inline UiLang uiLangGet() {
    return g_uiLang;
}

#if UI_LANG_VI
static const char* const kUiStrVi[STR_COUNT] = {
    "START", "Chế độ", "Cài đặt", "IDLE", "Thủ Công", "RANG",
    "QUAY LẠI", "Gia Nhiệt", "Quạt Gió", "Trống", "Quạt Mát", "END", "Xong", "OFF",
    "Xác Nhận", "Chắc chắn?", "WIFI", "Web portal", "Ngôn Ngữ",
    "Lưu", "Hủy", "Mặc định", "Khởi động lại", "Tắt nguồn", "Rang độc lập", "Connected", "Waiting",
    "QUÁ NHIỆT", "MẤT TÍN HIỆU NHIỆT", "Đã cắt gia nhiệt", "Kiểm tra kết nối",
    "Bỏ Qua", "HOME", "ĐỊA CHỈ IP", "Kết thúc mẻ rang?", "Xả hạt?",
    "HỆ THỐNG", "MÀN CHỜ", "ĐỘ SÁNG", "Tắt Gia Nhiệt", "NHIỆT ĐỘ",
    "THỦ CÔNG", "CÀI ĐẶT", "IDLE", "ÂM LƯỢNG"
};
#endif

#if UI_LANG_EN
static const char* const kUiStrEn[STR_COUNT] = {
    "START", "Mode", "Setting", "IDLE", "Manual", "ROAST",
    "BACK", "Heater", "Air Fan", "Drum", "Cool Fan", "END", "Done", "OFF",
    "Confirm", "Sure?", "WIFI", "Web portal", "Language",
    "Save", "Cancel", "Default", "Reset", "Power Off", "Standalone", "Connected", "Waiting",
    "OVERHEAT", "NO TEMP SIGNAL", "Heater cut off", "Check the connection",
    "Dismiss", "HOME", "IP ADDRESS", "End roast?", "Drop beans?",
    "SYSTEM", "IDLE TIME", "BRIGHTNESS", "Heater Off", "TEMP",
    "MANUAL", "SETTING", "IDLE", "VOLUME"
};
#endif

#if UI_LANG_CN
static const char* const kUiStrCn[STR_COUNT] = {
    "开始", "模式", "设置", "待机", "手动", "烘焙",
    "返回", "加热", "风量", "滚筒", "冷却", "结束", "完成", "关",
    "确认", "确定?", "WIFI", "Web portal", "语言",
    "保存", "取消", "重置", "重启", "关机", "Standalone", "已连接", "等待",
    "过热", "无温度信号", "加热已切断", "请确认连接",
    "忽略", "HOME", "IP 地址", "结束烘焙?", "确认下豆?",
    "系统", "待机时间", "屏幕亮度", "关闭加热", "温度单位",
    "手动", "设置", "待机", "音量"
};
#endif

static inline const char* uiTr(UiStrId id) {
    if (id >= STR_COUNT) return "";
#if UI_LANG_VI
    if (g_uiLang == UILANG_VI) return kUiStrVi[id];
#endif
#if UI_LANG_EN
    if (g_uiLang == UILANG_EN) return kUiStrEn[id];
#endif
#if UI_LANG_CN
    if (g_uiLang == UILANG_CN) return kUiStrCn[id];
#endif
    return "";
}
