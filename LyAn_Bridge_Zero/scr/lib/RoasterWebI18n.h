/*
 * GPL v3
 * Chữ trên web bằng 3 ngôn ngữ EN | VI | 中文. Mỗi dòng: mã, tiếng Anh, tiếng Việt, tiếng Hoa.
 */
#pragma once

#define WL_EN 0
#define WL_VI 1
#define WL_CN 2

struct WtRow { const char *k; const char *t[3]; };
#define WTR(k, en, vi, cn) { k, { en, vi, cn } },

static const WtRow WT[] = {
WTR("c.home2", "Home", "Trang chính", "主页")
WTR("c.sep", "&middot;", "&middot;", "&middot;")
WTR("nav.wifi", "Setup WiFi", "Cài đặt WiFi", "WiFi 设置")
WTR("nav.artisan", "Config Artisan", "Cấu hình Artisan", "Artisan 配置")
WTR("ft.fw", "Firmware", "Firmware", "固件")
WTR("ft.about", "About", "Giới thiệu", "关于")
WTR("ft.ver", "Version", "Phiên bản", "版本")
WTR("idx.wifi", "WiFi", "WiFi", "WiFi")
WTR("idx.notconn", "not connected", "chưa nối", "未连接")
WTR("idx.addr", "Address", "Địa chỉ", "地址")
WTR("w.scanning", "Scanning... the page reloads in 2 seconds.", "Đang quét... trang tự tải lại sau 2 giây.", "正在扫描… 2 秒后自动刷新。")
WTR("w.pick", "Select a network (press Rescan if it is missing)", "Chọn mạng (thiếu thì bấm Quét lại)", "选择网络（没有请点击重新扫描）")
WTR("w.open", "[open]", "[mở]", "[开放]")
WTR("w.noscan", "No scan results yet. Press Rescan and wait a few seconds.", "Chưa có kết quả quét. Bấm Quét lại rồi đợi vài giây.", "暂无扫描结果。请点击重新扫描并等待几秒。")
WTR("w.pass", "WiFi password", "Mật khẩu WiFi", "WiFi 密码")
WTR("w.save", "Save and connect", "Lưu và kết nối", "保存并连接")
WTR("w.rescan", "Rescan", "Quét lại", "重新扫描")
WTR("w.rescanning", "Scanning...", "Đang quét...", "扫描中…")
WTR("w.saved", "Saved networks", "Mạng đã lưu", "已保存的网络")
WTR("w.del", "Delete", "Xóa", "删除")
WTR("w.joined", "Saved. The device is connecting.<br><br><span class='m s'>If you came in through the device's own access point, its address will change; next time use <b>lyanzero.local</b>.</span>", "Đã lưu. Máy đang kết nối.<br><br><span class='m s'>Nếu đang vào qua điểm phát riêng thì địa chỉ máy sẽ đổi; lần sau vào bằng <b>lyanzero.local</b>.</span>", "已保存。设备正在连接。<br><br><span class='m s'>如果您是通过设备自带热点进入的，设备地址会改变；下次请使用 <b>lyanzero.local</b>。</span>")
WTR("a.addr", "Address", "Địa chỉ", "地址")
WTR("a.fw", "Firmware", "Firmware", "固件")
WTR("a.dev", "Device", "Thiết bị", "设备")
WTR("pin.title", "PIN code", "Mã PIN", "PIN 码")
WTR("pin.enter", "Enter PIN", "Nhập mã PIN", "请输入 PIN")
WTR("pin.ok", "Confirm", "Xác nhận", "确认")
WTR("pin.bad", "Wrong PIN.", "Mã PIN không đúng.", "PIN 码错误。")
WTR("pin.lock", "Too many attempts. Wait 1 minute and try again.", "Sai nhiều lần. Đợi 1 phút rồi thử lại.", "错误次数过多，请 1 分钟后重试。")
WTR("pin.change", "Change PIN", "Đổi mã PIN", "修改 PIN 码")
WTR("pin.cur", "Current PIN", "Mã PIN hiện tại", "当前 PIN 码")
WTR("pin.new", "New PIN (4 to 10 digits)", "Mã PIN mới (4 đến 10 chữ số)", "新 PIN 码（4 到 10 位数字）")
WTR("pin.new2", "Repeat the new PIN", "Nhập lại mã PIN mới", "再次输入新 PIN 码")
WTR("pin.save", "Save PIN", "Lưu mã PIN", "保存 PIN 码")
WTR("pin.done", "<span class=g>PIN changed.</span> Log in again with the new PIN.", "<span class=g>Đã đổi mã PIN.</span> Hãy đăng nhập lại bằng mã mới.", "<span class=g>PIN 码已修改。</span>请用新 PIN 码重新登录。")
WTR("pin.e_fmt", "The PIN must be 4 to 10 digits.", "Mã PIN phải gồm 4 đến 10 chữ số.", "PIN 码必须是 4 到 10 位数字。")
WTR("pin.e_match", "The two new PINs do not match.", "Hai mã PIN mới không giống nhau.", "两次输入的新 PIN 码不一致。")
WTR("fac.title", "Factory reset", "Reset Factory", "恢复出厂设置")
WTR("fac.ask", "Reset to factory settings? The PIN returns to the default and all saved WiFi networks are erased.", "Reset về ban đầu? Mã PIN trở về mặc định và xoá hết các mạng WiFi đã lưu.", "恢复出厂设置？PIN 码将恢复默认，并清除所有已保存的 WiFi 网络。")
WTR("fac.yes", "Yes, reset", "Có, reset", "是，恢复")
WTR("fac.no", "No", "Không", "否")
WTR("fac.boot", "Now press the <b>BOOT</b> button on the board within 10 seconds to confirm. The light blinks white. Do nothing to cancel.", "Bây giờ hãy nhấn nút <b>BOOT</b> trên bo trong vòng 10 giây để xác nhận. Đèn nháy trắng. Không làm gì thì huỷ.", "现在请在 10 秒内按下板上的 <b>BOOT</b> 按钮确认。指示灯闪烁白色。不操作则取消。")
WTR("ft.pin", "PIN", "PIN", "PIN")
WTR("u.title", "Firmware update (OTA)", "Nạp firmware (OTA)", "固件升级 (OTA)")
WTR("u.busy", "<span class=r>The heater is on or PID is running.</span> Turn the heater off (Heater 0%, PID off), then update the firmware.", "<span class=r>Heater đang bật hoặc PID đang chạy.</span> Hãy tắt Heater (0%, tắt PID) rồi nạp firmware.", "<span class=r>加热器正在运行或 PID 已开启。</span>请先关闭加热（火力 0%，关闭 PID），再升级固件。")
WTR("u.choose", "Select the firmware file (.bin)", "Chọn file firmware (.bin) của chương trình", "选择固件文件 (.bin)")
WTR("u.btn", "Update firmware", "Nạp firmware", "升级固件")
WTR("u.note", "Written to the spare slot; the running one is untouched. The device restarts when done. If power is lost midway it keeps running the old version.", "Ghi vào ô trống, ô đang chạy giữ nguyên. Nạp xong máy tự khởi động lại. Mất điện giữa chừng thì máy vẫn chạy bản cũ.", "写入备用分区，正在运行的分区不受影响。完成后设备自动重启。中途断电则继续运行旧版本。")
WTR("u.run", "Current version", "Phiên bản hiện tại", "当前版本")
WTR("u.max", "Maximum", "Tối đa", "最大")
WTR("u.ok", "<span class=g>Update complete.</span> The device is restarting, wait about 10 seconds and reload the page.", "<span class=g>Nạp xong.</span> Máy đang khởi động lại, đợi khoảng 10 giây rồi tải lại trang.", "<span class=g>升级完成。</span>设备正在重启，请等待约 10 秒后刷新页面。")
WTR("u.err", "Error: ", "Lỗi: ", "错误：")
WTR("u.e_busy", "heater is on", "đang cấp nhiệt, không nạp được", "正在加热，无法升级")
WTR("u.e_open", "cannot open the spare slot", "không mở được ô trống", "无法打开备用分区")
WTR("u.e_write", "write error", "ghi lỗi", "写入错误")
WTR("u.e_bad", "invalid file", "file không hợp lệ", "文件无效")
WTR("u.e_abort", "upload interrupted", "tải lên bị ngắt", "上传被中断")
WTR("up.warn", "UPDATING - do NOT power off the device, do NOT close or leave this page!", "ĐANG CẬP NHẬT - KHÔNG tắt thiết bị, KHÔNG đóng hoặc rời trang này!", "正在更新 - 请勿关闭设备电源，请勿关闭或离开此页面！")
WTR("up.restart", "Done. The device is restarting... {0} s (the page reloads when it is back)", "Nạp xong. Máy đang khởi động lại... {0} giây (tự tải lại trang khi máy lên)", "完成。设备正在重启… {0} 秒（设备恢复后页面自动刷新）")
WTR("up.writing", "writing to memory, please wait...", "đang ghi vào bộ nhớ, vui lòng chờ...", "正在写入存储，请稍候…")
WTR("up.left", "about {0} s left", "còn khoảng {0} giây", "约剩 {0} 秒")
WTR("up.calc", "calculating...", "đang tính...", "计算中…")
WTR("up.neterr", "Connection error. Check the device and try again (reload the page).", "Lỗi kết nối. Kiểm tra máy rồi thử lại (tải lại trang).", "连接错误。请检查设备后重试（刷新页面）。")
WTR("ar.addr", "Device address", "Địa chỉ máy rang", "设备地址")
WTR("ar.dl", "Download Artisan settings file", "Tải file cài đặt Artisan", "下载 Artisan 设置文件")
WTR("ar.dlnote", "Artisan: Help &rarr; Load Settings &rarr; choose the downloaded file. The file holds the full LyAn Roaster configuration (curve colors, control buttons, sliders, WebSocket) and replaces your current Artisan settings. Press Help &rarr; Save Settings first to keep your old configuration.", "Artisan: Help &rarr; Load Settings &rarr; chọn file vừa tải. File chứa toàn bộ cấu hình LyAn Roaster (màu đường cong, nút điều khiển, thanh trượt, WebSocket) và sẽ thay cài đặt Artisan hiện có của bạn. Nên bấm Help &rarr; Save Settings để lưu lại cấu hình cũ trước.", "Artisan：Help &rarr; Load Settings &rarr; 选择刚下载的文件。该文件包含完整的 LyAn Roaster 配置（曲线颜色、控制按钮、滑块、WebSocket），会替换您当前的 Artisan 设置。请先点击 Help &rarr; Save Settings 保存原有配置。")
WTR("ar.after", "After loading", "Sau khi nạp", "加载之后")
WTR("ar.steps", "1. Connect your computer to the device WiFi (<b>LyAn_Roaster_Zero</b>) or to the same network as the device. When Artisan connects, Bluetooth (HiBean) turns off by itself.<br>2. In Artisan press ON to start recording. The bean temperature (BT) will run.<br>3. Use the row of 10 control buttons (Heater, Air, Drum, Cool, PID) that is already in the file: see below.<br>No START button is needed: the heater can be controlled as soon as Artisan is connected.", "1. Nối máy tính vào WiFi của máy rang (<b>LyAn_Roaster_Zero</b>) hoặc cùng mạng WiFi với máy. Khi Artisan nối vào, Bluetooth (HiBean) tự tắt.<br>2. Trong Artisan bấm ON để bật ghi. Nhiệt độ hạt (BT) sẽ chạy.<br>3. Dùng hàng 10 nút điều khiển (Heater, Air, Drum, Cool, PID) nằm sẵn trong file: xem mục bên dưới.<br>Không cần bấm START: điều khiển được Heater ngay khi Artisan đã nối.", "1. 将电脑连接到设备的 WiFi（<b>LyAn_Roaster_Zero</b>）或与设备处于同一网络。Artisan 连接后，蓝牙（HiBean）会自动关闭。<br>2. 在 Artisan 中点击 ON 开始记录，豆温 (BT) 开始变化。<br>3. 使用文件中已包含的 10 个控制按钮（Heater、Air、Drum、Cool、PID）：见下方说明。<br>无需按 START：Artisan 连接后即可直接控制加热。")
WTR("ar.btns", "Control buttons", "Hàng nút điều khiển", "控制按钮")
WTR("ar.btnstxt", "The file includes 10 buttons: HEATER -/+, AIR -/+, DRUM ON/OFF, COOL ON/OFF, PID ON/OFF. They appear in the main window after you press <b>ON</b> in Artisan. Each HEATER / AIR press changes the value by 5%.<br>Artisan's own event buttons (CHARGE, DRY END, FC START, SC START, DROP) only mark the Artisan chart and do not control the device.", "File có sẵn 10 nút: HEATER -/+, AIR -/+, DRUM ON/OFF, COOL ON/OFF, PID ON/OFF. Nút hiện trên cửa sổ chính sau khi bấm <b>ON</b> ở Artisan. Mỗi lần bấm HEATER / AIR đổi 5%.<br>Nút sự kiện gốc của Artisan (CHARGE, DRY END, FC START, SC START, DROP) chỉ ghi vào đồ thị Artisan, không điều khiển máy.", "文件包含 10 个按钮：HEATER -/+、AIR -/+、DRUM ON/OFF、COOL ON/OFF、PID ON/OFF。在 Artisan 中点击 <b>ON</b> 后显示在主窗口。每次点击 HEATER / AIR 变化 5%。<br>Artisan 自带的事件按钮（CHARGE、DRY END、FC START、SC START、DROP）只标记 Artisan 曲线图，不控制设备。")
WTR("cl.title", "Update from Cloud", "Cập nhật từ Cloud", "云端升级")
WTR("cl.new", "New firmware available: {0}", "Có firmware mới: {0}", "有新固件：{0}")
WTR("cl.tap", "Tap to view and update", "Bấm để xem và cập nhật", "点击查看并升级")
WTR("cl.size", "File size", "Dung lượng", "文件大小")
WTR("cl.notes", "Notes", "Ghi chú", "说明")
WTR("cl.btn", "Download and install", "Tải về và cài đặt", "下载并安装")
WTR("cl.recheck", "Check again", "Kiểm tra lại", "重新检查")
WTR("cl.uptodate", "You have the latest version.", "Đang là bản mới nhất.", "已是最新版本。")
WTR("cl.checking", "Checking...", "Đang kiểm tra...", "正在检查…")
WTR("cl.idle", "Not checked yet. Connect the device to a WiFi network with internet (Setup WiFi), then press Check again.", "Chưa kiểm tra. Hãy nối máy vào WiFi có internet (Cài đặt WiFi), rồi bấm Kiểm tra lại.", "尚未检查。请将设备连接到可上网的 WiFi（WiFi 设置），然后点击重新检查。")
WTR("cl.neterr", "Cannot reach the update server. Check that the WiFi network has internet.", "Không kết nối được máy chủ cập nhật. Kiểm tra WiFi có internet không.", "无法连接升级服务器。请检查 WiFi 是否可以上网。")
WTR("cl.fmt", "The update information on the server is invalid.", "Thông tin cập nhật trên máy chủ không hợp lệ.", "服务器上的升级信息无效。")
WTR("cl.e_http", "download failed", "tải về thất bại", "下载失败")
WTR("cl.e_size", "wrong file size", "sai dung lượng file", "文件大小不符")
WTR("cl.e_md5", "checksum (MD5) mismatch, the file is damaged", "sai mã kiểm tra MD5, file bị hỏng", "MD5 校验不符，文件已损坏")
WTR("cl.e_mem", "not enough free memory right now, try again", "thiếu bộ nhớ trống lúc này, hãy thử lại", "当前可用内存不足，请重试")
};
#define WT_COUNT ((int)(sizeof(WT) / sizeof(WT[0])))

static int wlFromCode(const String &v) {
    if (v == "en") return WL_EN;
    if (v == "vi") return WL_VI;
    if (v == "cn" || v == "zh") return WL_CN;
    return -1;
}
static const char *wlCode(int l) { return l == WL_VI ? "vi" : (l == WL_CN ? "cn" : "en"); }
// Mã ngôn ngữ cho thẻ <html lang>
static const char *wlHtmlLang(int l) { return l == WL_VI ? "vi" : (l == WL_CN ? "zh" : "en"); }

// Ngôn ngữ: ?l=xx > cookie "lang" > tiếng Việt
static int wl() {
    int r = -1;
    if (g_web.hasArg("l")) r = wlFromCode(g_web.arg("l"));
    if (r < 0) {
        const String c = g_web.header("Cookie");
        const int i = c.indexOf("lang=");
        if (i >= 0) r = wlFromCode(c.substring(i + 5, i + 7));
    }
    return r < 0 ? WL_VI : r;
}

// Chữ theo ngôn ngữ hiện tại; thiếu thì trả lại mã khoá
static String wt(const char *k) {
    const int l = wl();
    for (int i = 0; i < WT_COUNT; i++)
        if (!strcmp(WT[i].k, k)) return String(WT[i].t[l]);
    return String(k);
}
// Chuỗi cho JavaScript (giữ nguyên {0})
static String wtJs(const char *k) {
    String s = wt(k);
    s.replace("\\", "\\\\");
    s.replace("'", "\\'");
    s.replace("\n", " ");
    s.replace("\r", "");
    return s;
}
