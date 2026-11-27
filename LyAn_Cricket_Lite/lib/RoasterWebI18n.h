/*
 * lib/RoasterWebI18n.h - Web Portal Multi-language Dictionary (EN | VI | CN) (Tier 5)
 *
 * Included by lib/RoasterWeb.h after g_web is declared.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

// WT_FS = 1 khi co kho file (FFat) de doc /www/i18n/*.json va /www/style.css. V3: chi khi module Fs (HAS_MODULE_FS, nhom sau) da duoc include truoc file nay.

#define WL_EN 0
#define WL_VI 1
#define WL_CN 2

static uint8_t (*g_webLangCb)() = nullptr;      // ngon ngu cai tren may (0 EN, 1 VI, 2 CN)

struct WtRow { const char *k; const char *t[3]; };
#define WTR(k, en, vi, cn) { k, { en, vi, cn } },

static const WtRow WT[] = {
// ---- chung
WTR("c.home2",  "Home",                            "Trang chính",                        "主页")
WTR("c.logout", "Log out",                         "Đăng xuất",                          "退出登录")
WTR("c.sep",    "&middot;",                        "&middot;",                           "&middot;")
// ---- trang chinh
WTR("nav.wifi",    "Setup WiFi",                   "Cài đặt WiFi",                       "WiFi 设置")
WTR("nav.artisan", "Config Artisan",               "Cấu hình Artisan",                   "Artisan 配置")
WTR("ft.about",    "About",                        "Giới thiệu",                         "关于")
WTR("ft.ver",      "Version",                      "Phiên bản",                          "版本")
WTR("idx.wifi",    "WiFi",                         "WiFi",                               "WiFi")
WTR("idx.notconn", "not connected",                "chưa nối",                           "未连接")
WTR("idx.addr",    "Address",                      "Địa chỉ",                            "地址")
WTR("idx.time",    "Time",                         "Giờ",                                "时间")
// ---- WiFi
WTR("w.scanning",  "Scanning... the page reloads in 2 seconds.", "Đang quét... trang tự tải lại sau 2 giây.", "正在扫描… 2 秒后自动刷新。")
WTR("w.pick",      "Select a network (press Rescan if it is missing)", "Chọn mạng (thiếu thì bấm Quét lại)", "选择网络（没有请点击重新扫描）")
WTR("w.open",      "[open]",                       "[mở]",                               "[开放]")
WTR("w.noscan",    "No scan results yet. Press Rescan and wait a few seconds.", "Chưa có kết quả quét. Bấm Quét lại rồi đợi vài giây.", "暂无扫描结果。请点击重新扫描并等待几秒。")
WTR("w.pass",      "WiFi password",                "Mật khẩu WiFi",                      "WiFi 密码")
WTR("w.save",      "Connect",                      "Kết nối",                            "连接")
WTR("w.rescan",    "Rescan",                       "Quét lại",                           "重新扫描")
WTR("w.rescanning","Scanning...",                  "Đang quét...",                       "扫描中…")
WTR("w.saved",     "Saved networks",               "Mạng đã lưu",                        "已保存的网络")
WTR("w.def",       "(default)",                    "(mặc định)",                         "（默认）")
WTR("w.del",       "Delete",                       "Xóa",                                "删除")
WTR("w.joined",    "Saved. The device is connecting - see the result on the Network page of the screen.<br><br><span class='m s'>If you came in through the device's own access point, its address will change; next time use <b>lyan.local</b>.</span>",
                   "Đã lưu. Máy đang kết nối - xem kết quả trên trang Network của màn hình.<br><br><span class='m s'>Nếu đang vào qua điểm phát riêng thì địa chỉ máy sẽ đổi; lần sau vào bằng <b>lyan.local</b>.</span>",
                   "已保存。设备正在连接 - 请在屏幕的网络页面查看结果。<br><br><span class='m s'>如果您是通过设备自带热点进入的，设备地址会改变；下次请使用 <b>lyan.local</b>。</span>")
// ---- about
WTR("a.phone",     "Phone",                        "Điện thoại",                         "电话")
WTR("a.addr",      "Address",                      "Địa chỉ",                            "地址")
WTR("a.fw",        "Firmware",                     "Firmware",                           "固件")
WTR("a.dev",       "Device",                       "Thiết bị",                           "设备")
// ---- PIN
WTR("pin.enter",   "Enter PIN",                    "Nhập mã PIN",                        "请输入 PIN")
WTR("pin.ok",      "Confirm",                      "Xác nhận",                           "确认")
WTR("pin.bad",     "Wrong PIN.",                   "Mã PIN không đúng.",                 "PIN 码错误。")
WTR("pin.lock",    "Too many attempts. Wait 1 minute and try again.", "Sai nhiều lần. Đợi 1 phút rồi thử lại.", "错误次数过多，请 1 分钟后重试。")
// ---- Config Artisan
WTR("ar.addr",     "Device address",               "Địa chỉ máy rang",                   "设备地址")
WTR("ar.dl",       "Download Artisan settings file", "Tải file cài đặt Artisan",         "下载 Artisan 设置文件")
WTR("ar.dlnote",   "Artisan: Help &rarr; Load Settings &rarr; choose the downloaded file. The file holds the full LyAn Roaster configuration (curve colors, control buttons, sliders, WebSocket) and replaces your current Artisan settings. Press Help &rarr; Save Settings first to keep your old configuration.",
                   "Artisan: Help &rarr; Load Settings &rarr; chọn file vừa tải. File chứa toàn bộ cấu hình LyAn Roaster (màu đường cong, nút điều khiển, thanh trượt, WebSocket) và sẽ thay cài đặt Artisan hiện có của bạn. Nên bấm Help &rarr; Save Settings để lưu lại cấu hình cũ trước.",
                   "Artisan：Help &rarr; Load Settings &rarr; 选择刚下载的文件。该文件包含完整的 LyAn Roaster 配置（曲线颜色、控制按钮、滑块、WebSocket），会替换您当前的 Artisan 设置。请先点击 Help &rarr; Save Settings 保存原有配置。")
WTR("ar.dev",      "Developer: the device sends /aset/LyAn Roaster.aset if it exists (edit it in Files), otherwise the built-in copy. Only the WebSocket host and port are filled in when sending.",
                   "Nhà phát triển: máy gửi file /aset/LyAn Roaster.aset nếu có (sửa trong Tập tin), không có thì dùng bản nhúng. Chỉ host và port của WebSocket được điền lúc gửi.",
                   "开发者：设备优先发送 /aset/LyAn Roaster.aset（可在“文件”中编辑），没有则使用内置版本。发送时只会填入 WebSocket 的主机和端口。")
WTR("ar.after",    "After loading",                "Sau khi nạp",                        "加载之后")
WTR("ar.steps",    "1. On the device screen press the <b>Artisan</b> button (HiBean turns off - they are mutually exclusive).<br>2. In Artisan press ON to start recording. The bean temperature (BT) will run.<br>3. Press Charge on the device: Artisan marks CHARGE by itself. DE / 1FC / 2FC / Drop are marked automatically too.<br>4. The row of 10 control buttons (Heater, Air, Drum, Cool, PID) is already in the file: see below.",
                   "1. Trên màn hình máy rang bấm nút <b>Artisan</b> (HiBean sẽ tự tắt - hai cái loại trừ nhau).<br>2. Artisan: ON để bật ghi. Nhiệt độ hạt (BT) sẽ chạy.<br>3. Bấm Charge trên máy: Artisan tự đánh dấu CHARGE. DE / 1FC / 2FC / Drop cũng tự đánh dấu.<br>4. Hàng 10 nút điều khiển (Heater, Air, Drum, Cool, PID) nằm sẵn trong file: xem mục bên dưới.",
                   "1. 在设备屏幕上点击 <b>Artisan</b> 按钮（HiBean 会自动关闭 - 两者互斥）。<br>2. 在 Artisan 中点击 ON 开始记录，豆温 (BT) 开始变化。<br>3. 在设备上按 Charge：Artisan 自动标记 CHARGE，DE / 1FC / 2FC / Drop 也会自动标记。<br>4. 10 个控制按钮（Heater、Air、Drum、Cool、PID）已包含在文件中：见下方说明。")
WTR("ar.btns",     "Control buttons",              "Hàng nút điều khiển",                "控制按钮")
WTR("ar.btnstxt",  "The file includes 10 buttons: HEATER -/+, AIR -/+, DRUM ON/OFF, COOL ON/OFF, PID ON/OFF. They appear in the main window after you press <b>ON</b> in Artisan. Each HEATER / AIR press changes the value by 5%.<br>Artisan's own event buttons (CHARGE, DRY END, FC START, SC START, DROP) only mark the Artisan chart and do not control the device. Marks pressed on the device are reported to Artisan automatically.",
                   "File có sẵn 10 nút: HEATER -/+, AIR -/+, DRUM ON/OFF, COOL ON/OFF, PID ON/OFF. Nút hiện trên cửa sổ chính sau khi bấm <b>ON</b> ở Artisan. Mỗi lần bấm HEATER / AIR đổi 5%.<br>Nút sự kiện gốc của Artisan (CHARGE, DRY END, FC START, SC START, DROP) chỉ ghi vào đồ thị Artisan, không điều khiển máy. Các mốc bấm trên máy thì máy tự báo lên Artisan.",
                   "文件包含 10 个按钮：HEATER -/+、AIR -/+、DRUM ON/OFF、COOL ON/OFF、PID ON/OFF。在 Artisan 中点击 <b>ON</b> 后显示在主窗口。每次点击 HEATER / AIR 变化 5%。<br>Artisan 自带的事件按钮（CHARGE、DRY END、FC START、SC START、DROP）只标记 Artisan 曲线图，不控制设备。在设备上按下的节点会自动通知 Artisan。")
};
#define WT_COUNT ((int)(sizeof(WT) / sizeof(WT[0])))

// ---------------------------------------------------------------- chon ngon ngu
static int wlFromCode(const String &v) {
    if (v == "en") return WL_EN;
    if (v == "vi") return WL_VI;
    if (v == "cn" || v == "zh") return WL_CN;           // "zh" cu (cookie / duong dan cua v2.9.4) van nhan
    return -1;
}
static const char *wlCode(int l) { return l == WL_VI ? "vi" : (l == WL_CN ? "cn" : "en"); }
// Ma ngon ngu cua THE <html lang=...> (chuan trinh duyet / doc man hinh): tieng Hoa la "zh", khong phai "cn"
static const char *wlHtmlLang(int l) { return l == WL_VI ? "vi" : (l == WL_CN ? "zh" : "en"); }

// Ngon ngu cua yeu cau dang xu ly: ?l=xx > cookie "lang" > ngon ngu dang cai tren may > VI.
static int wl() {
    int r = -1;
    if (g_web.hasArg("l")) r = wlFromCode(g_web.arg("l"));
    if (r < 0) {
        const String c = g_web.header("Cookie");
        int i = 0;
        while ((i = c.indexOf("lang=", i)) >= 0) {
            if (i == 0 || c[i - 1] == ' ' || c[i - 1] == ';') {
                r = wlFromCode(c.substring(i + 5, i + 7));
                break;
            }
            i += 5;
        }
    }
    if (r < 0) {
        const int d = g_webLangCb ? (int)g_webLangCb() : WL_VI;
        r = (d >= WL_EN && d <= WL_CN) ? d : WL_VI;
    }
    return r;
}

// ---------------------------------------------------------------- ghi de tu /www/i18n/<xx>.json


// Chu theo ngon ngu hien tai; thieu trong ca bang lan file -> tra lai chinh ma khoa de de thay loi.
static String wtL(int l, const char *k) {
    String o;
    for (int i = 0; i < WT_COUNT; i++)
        if (!strcmp(WT[i].k, k)) return String(WT[i].t[l]);
    return String(k);
}
static String wt(const char *k) { return wtL(wl(), k); }

