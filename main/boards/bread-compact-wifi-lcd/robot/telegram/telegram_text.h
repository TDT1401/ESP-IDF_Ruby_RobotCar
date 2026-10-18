#pragma once

// Edit this file to customize Telegram wording and icons.
// Messages use Telegram HTML: supported tags include <b>, <i>, <code> and <a>.
// Keep every opened tag closed. Dynamic values (IP, firmware, status) are added
// by telegram.cc and should not be written into these strings.
namespace L298nTelegramText {

inline constexpr char kDivider[] = "━━━━━━━━━━━━━━━━";

inline constexpr char kStatusHeader[] = "<b>🤖 XiaoMin ROBOT</b>";
inline constexpr char kSystemSection[] = "<b>🧠 HỆ THỐNG</b>";
inline constexpr char kDeviceSection[] = "<b>🔌 THIẾT BỊ</b>";
inline constexpr char kMotorSection[] = "<b>🚗 ĐỘNG CƠ</b>";
inline constexpr char kNavigationSection[] = "<b>📡 HC-SR04 / TỰ HÀNH</b>";
inline constexpr char kNetworkSection[] = "<b>📶 KẾT NỐI</b>";
inline constexpr char kStatusFooter[] =
    "<i>Motor hiển thị lệnh/PWM, không phải tốc độ đo từ bánh xe. Màn hình, motor, loa và mic "
    "không có đường nhận dạng thiết bị; trạng thái chỉ xác nhận phía firmware.</i>";
inline constexpr char kStateLabel[] = "• Trạng thái: ";
inline constexpr char kFirmwareLabel[] = "• Firmware: ";
inline constexpr char kUptimeLabel[] = "• Hoạt động: ";
inline constexpr char kControlLabel[] = "• Điều khiển: ";
inline constexpr char kDirectionLabel[] = "• Hướng: ";
inline constexpr char kMotionStateLabel[] = "• Hoạt động: ";
inline constexpr char kActivePwmLabel[] = "• PWM hiện tại: ";
inline constexpr char kSavedSpeedLabel[] = "• Tốc độ đã đặt: ";
inline constexpr char kEmotionLabel[] = "• Chuyển động cảm xúc: ";
inline constexpr char kNavigationModeLabel[] = "• Chế độ: ";
inline constexpr char kFollowDistanceLabel[] = "• Khoảng cách follow: ";
inline constexpr char kObstacleDistanceLabel[] = "• Khoảng cách né vật cản: ";
inline constexpr char kBlockedLabel[] = "• Đường đi: ";
inline constexpr char kHardwareLabel[] = "• Phần cứng: ";
inline constexpr char kDisplayDeviceLabel[] = "• Màn hình: ";
inline constexpr char kMotorDeviceLabel[] = "• Motor: ";
inline constexpr char kSpeakerDeviceLabel[] = "• Loa: ";
inline constexpr char kMicrophoneDeviceLabel[] = "• Mic: ";
inline constexpr char kSr04DeviceLabel[] = "• HC-SR04: ";
inline constexpr char kWifiLabel[] = "• Wi-Fi: ";
inline constexpr char kSignalLabel[] = "• Tín hiệu: ";
inline constexpr char kIpLabel[] = "• IP: ";
inline constexpr char kFreeRamLabel[] = "• RAM trống: ";
inline constexpr char kWebOpenText[] = "Mở trang điều khiển xe";
inline constexpr char kWebUnavailableStatus[] = "🌐 Trang điều khiển web chưa sẵn sàng.";
inline constexpr char kWebLinkPrefix[] = "🔗 ";
inline constexpr char kStatusWebLinkPrefix[] = "🌐 ";
inline constexpr char kWebAddressLabel[] = "📍 Địa chỉ: ";
inline constexpr char kBootFirmwareLabel[] = "📦 Firmware: ";
inline constexpr char kHardwareErrorPrefix[] = "❌ ";
inline constexpr char kDeviceReady[] = "✅ Sẵn sàng";
inline constexpr char kDeviceActive[] = "✅ Đang hoạt động";
inline constexpr char kDeviceNoInfo[] = "❌ Lỗi: không có thông tin";
inline constexpr char kDisplayNotReady[] = "❌ Lỗi: giao diện chưa khởi tạo";
inline constexpr char kSr04NoEcho[] = "❌ Lỗi: không nhận được Echo";
inline constexpr char kSignalUnitSeparator[] = " dBm · ";
inline constexpr char kMemoryUnit[] = " KB";

inline constexpr char kUptimeFormat[] = "%lld giờ %lld phút %lld giây";
inline constexpr char kControlWeb[] = "🎮 Web thủ công";
inline constexpr char kControlVoice[] = "🎙️ Giọng nói";
inline constexpr char kControlAutonomy[] = "🤖 Tự hành";
inline constexpr char kNavigationManual[] = "Thủ công";
inline constexpr char kNavigationFollow[] = "Follow";
inline constexpr char kNavigationAvoid[] = "Né vật cản";
inline constexpr char kPathClear[] = "Sẵn sàng";
inline constexpr char kPathBlocked[] = "Bị chặn, motor đã dừng";
inline constexpr char kMotorRunning[] = "🟢 Đang chạy";
inline constexpr char kMotorPending[] = "🟡 Đang chờ đổi chiều";
inline constexpr char kMotorIdle[] = "⚪ Đang dừng";
inline constexpr char kEmotionOn[] = "✅ Bật";
inline constexpr char kEmotionOff[] = "⛔ Tắt";
inline constexpr char kEmotionActiveSuffix[] = " · đang hoạt động";
inline constexpr char kHardwareOk[] = "✅ Bình thường";
inline constexpr char kWifiConnected[] = "✅ Đã kết nối";
inline constexpr char kWifiDisconnected[] = "❌ Đã ngắt";
inline constexpr char kRssiExcellent[] = "Rất tốt";
inline constexpr char kRssiGood[] = "Tốt";
inline constexpr char kRssiFair[] = "Trung bình";
inline constexpr char kRssiWeak[] = "Yếu";

inline constexpr char kStateStarting[] = "Đang khởi động";
inline constexpr char kStateWifiConfiguring[] = "Đang cấu hình Wi-Fi";
inline constexpr char kStateIdle[] = "Sẵn sàng";
inline constexpr char kStateConnecting[] = "Đang kết nối máy chủ";
inline constexpr char kStateListening[] = "Đang lắng nghe";
inline constexpr char kStateSpeaking[] = "Đang nói";
inline constexpr char kStateNotifying[] = "Đang thông báo";
inline constexpr char kStateUpgrading[] = "Đang cập nhật firmware";
inline constexpr char kStateActivating[] = "Đang kích hoạt";
inline constexpr char kStateAudioTesting[] = "Đang kiểm tra âm thanh";
inline constexpr char kStateFatalError[] = "Lỗi nghiêm trọng";
inline constexpr char kStateUnknown[] = "Chưa xác định";
inline constexpr char kStateIconIdle[] = "🟢";
inline constexpr char kStateIconListening[] = "👂";
inline constexpr char kStateIconSpeaking[] = "🗣️";
inline constexpr char kStateIconFatalError[] = "🔴";
inline constexpr char kStateIconUpgrading[] = "⬆️";
inline constexpr char kStateIconDefault[] = "🟡";

inline constexpr char kMotionForward[] = "⬆️ Tiến";
inline constexpr char kMotionBackward[] = "⬇️ Lùi";
inline constexpr char kMotionLeft[] = "↩️ Xoay trái";
inline constexpr char kMotionRight[] = "↪️ Xoay phải";
inline constexpr char kMotionStopped[] = "⏹️ Đứng yên";

inline constexpr char kMenu[] =
    "<b>🤖 XiaoMin ROBOT</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "⬆️ /forward CM — Tiến theo quãng đường; /backward CM — Lùi theo quãng đường (1-100 cm)\n"
    "<b>📋 DANH SÁCH LỆNH</b>\n\n"
    "🎙️ /wake — Đánh thức XiaoMin để trò chuyện\n"
    "⚙️ /speed — Xem tốc độ; /speed 50 — Đặt PWM 50%\n"
    "👣 /follow on|off|CM — Follow; mặc định 30 cm\n"
    "🧭 /avoid on|off|CM — Tự hành né vật cản; mặc định 20 cm\n"
    "⏹️ /stop — Dừng motor và tắt tự hành\n"
    "🔎 /status — Xem trạng thái XiaoMin\n"
    "🌐 /web — Mở trang điều khiển xe\n"
    "📖 /menu — Hiển thị menu này\n\n"
    "<i>Sau khi đánh thức, hãy nói vào mic trên XiaoMin. Điều khiển xe thực hiện trên trang web "
    "hoặc bằng giọng nói.</i>";

inline constexpr char kWakeRequested[] =
    "🎙️ Đã gửi yêu cầu đánh thức. Chờ XiaoMin chuyển sang lắng nghe rồi nói vào mic trên XiaoMin.\n"
    "Gửi /status để kiểm tra nếu XiaoMin chưa phản hồi.";
inline constexpr char kWakeAlreadyListening[] =
    "👂 XiaoMin đang lắng nghe. Bạn có thể nói vào mic trên XiaoMin.";
inline constexpr char kWakeConnecting[] =
    "⏳ XiaoMin đang kết nối máy chủ. Hãy chờ XiaoMin chuyển sang lắng nghe.";
inline constexpr char kWakeSpeaking[] =
    "🗣️ XiaoMin đang nói. Hãy chờ XiaoMin nói xong và chuyển sang lắng nghe.";
inline constexpr char kWakeUnavailable[] =
    "⚠️ XiaoMin chưa sẵn sàng để đánh thức. Gửi /status để xem trạng thái và thử lại khi XiaoMin sẵn "
    "sàng.";

inline constexpr char kSpeedCurrentPrefix[] = "⚙️ Tốc độ đã đặt: ";
inline constexpr char kSpeedUsage[] =
    "Dùng <code>/speed N</code> với N là số nguyên từ 0 đến 100, ví dụ <code>/speed 50</code>.\n"
    "0 dừng motor khi ở chế độ giọng nói. Khi web đang giữ quyền, hãy bấm “Trả về giọng nói” "
    "trước.\n"
    "Đây là mức PWM, không phải tốc độ đo từ bánh xe. Cài đặt chỉ giữ đến khi khởi động lại.";
inline constexpr char kSpeedSetPrefix[] = "✅ Đã đặt tốc độ motor (PWM): ";
inline constexpr char kSpeedSetFooter[] =
    "Áp dụng ngay cho chuyển động giọng nói đang chạy, không kéo dài thời gian chạy. "
    "Motor đang dừng sẽ không tự chạy; mức 0 dừng motor.\n"
    "Cài đặt chỉ giữ đến khi khởi động lại.";
inline constexpr char kSpeedWebBusy[] =
    "🎮 Web đang giữ quyền điều khiển motor. Hãy bấm “Trả về giọng nói” trên web rồi gửi lại "
    "/speed.";
inline constexpr char kSpeedHardwareError[] =
    "❌ Có lỗi phần cứng motor khi đặt tốc độ. Gửi /status để kiểm tra, kiểm tra dây nối và khởi "
    "động lại.";

inline constexpr char kDistanceMoveUsage[] =
    "Dùng <code>/forward CM</code> hoặc <code>/backward CM</code> với CM từ 1 đến 100, ví dụ: "
    "<code>/forward 30</code>. Lệnh cần hai encoder FC-03 đã được hiệu chuẩn.";
inline constexpr char kDistanceMoveAccepted[] =
    "✅ Đã bắt đầu chạy theo quãng đường encoder. Gửi /status để xem tiến độ; motor tự dừng khi đạt mục tiêu.";
inline constexpr char kDistanceMoveWebBusy[] =
    "🎮 Web đang giữ quyền điều khiển motor. Hãy trả quyền trên trang web rồi gửi lại lệnh.";
inline constexpr char kDistanceMoveUnavailable[] =
    "❌ Không thể chạy theo quãng đường: FC-03 chưa sẵn sàng/hiệu chuẩn, PWM đang bằng 0, hoặc motor không thể chạy. Kiểm tra /status.";

inline constexpr char kFollowUsage[] =
    "Dùng <code>/follow on</code>, <code>/follow off</code> hoặc <code>/follow N</code> với N từ "
    "10 đến 200 cm. Ví dụ: <code>/follow 30</code>.";
inline constexpr char kAvoidUsage[] =
    "Dùng <code>/avoid on</code>, <code>/avoid off</code> hoặc <code>/avoid N</code> với N từ 10 "
    "đến 100 cm. Ví dụ: <code>/avoid 20</code>.";
inline constexpr char kAutonomyUnavailable[] =
    "❌ Không thể bật tự hành khi web đang điều khiển, tốc độ đang bằng 0 hoặc motor có lỗi.";
inline constexpr char kEmergencyStopped[] = "⏹️ Motor đã dừng và chế độ tự hành đã tắt.";

inline constexpr char kWebReadyHeader[] =
    "<b>🌐 ĐIỀU KHIỂN XE</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "✅ Trang điều khiển đã sẵn sàng.\n\n";
inline constexpr char kWebReadyFooter[] =
    "\n\n<i>Thiết bị mở link cần truy cập được cùng mạng Wi-Fi hoặc hotspot với XiaoMin.</i>";
inline constexpr char kWebWifiDisconnected[] =
    "<b>🌐 ĐIỀU KHIỂN XE</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "❌ XiaoMin đã mất kết nối Wi-Fi.\n\n"
    "Hãy chờ XiaoMin kết nối lại rồi gửi /web lần nữa.";
inline constexpr char kWebServerUnavailable[] =
    "<b>🌐 ĐIỀU KHIỂN XE</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "⚠️ Wi-Fi đã kết nối nhưng web server chưa sẵn sàng.\n\n"
    "Vui lòng thử lại sau vài giây hoặc kiểm tra log <code>L298nWeb</code>.";

inline constexpr char kBootHeader[] =
    "<b>🚀 XiaoMin ĐÃ SẴN SÀNG</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "✅ XiaoMin đã khởi động và kết nối Wi-Fi.\n";
inline constexpr char kIpChangedHeader[] =
    "<b>🔄 ĐỊA CHỈ XiaoMin ROBOT ĐÃ THAY ĐỔI</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "✅ XiaoMin Robot đã kết nối lại mạng với địa chỉ mới.\n";
inline constexpr char kBootFooter[] =
    "\n\nGửi /status để xem tình trạng XiaoMin hoặc /menu để xem các lệnh.";

inline constexpr char kUnknownCommand[] =
    "<b>❓ LỆNH CHƯA ĐƯỢC HỖ TRỢ</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "Mình chưa hiểu lệnh này. Gửi /menu để xem danh sách lệnh hiện có.";

// Telegram reply keyboard. Keep this as valid JSON.
inline constexpr char kKeyboardJson[] =
    R"({"keyboard":[["/wake","/stop"],["/follow on","/follow off"],["/avoid on","/avoid off"],["/status","/web"],["/menu"]],"resize_keyboard":true})";

}  // namespace L298nTelegramText
