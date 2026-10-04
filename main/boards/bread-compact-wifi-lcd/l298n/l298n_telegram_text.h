#pragma once

// Edit this file to customize Telegram wording and icons.
// Messages use Telegram HTML: supported tags include <b>, <i>, <code> and <a>.
// Keep every opened tag closed. Dynamic values (IP, firmware, status) are added
// by l298n_telegram.cc and should not be written into these strings.
namespace L298nTelegramText {

inline constexpr char kDivider[] = "━━━━━━━━━━━━━━━━";

inline constexpr char kStatusHeader[] = "<b>🤖 XIAOZHI ROBOT</b>";
inline constexpr char kSystemSection[] = "<b>🧠 HỆ THỐNG</b>";
inline constexpr char kMotorSection[] = "<b>🚗 ĐỘNG CƠ</b>";
inline constexpr char kNetworkSection[] = "<b>📶 KẾT NỐI</b>";
inline constexpr char kStatusFooter[] =
    "<i>Motor hiển thị lệnh/PWM, không phải tốc độ đo từ bánh xe.</i>";
inline constexpr char kStateLabel[] = "• Trạng thái: ";
inline constexpr char kFirmwareLabel[] = "• Firmware: ";
inline constexpr char kUptimeLabel[] = "• Hoạt động: ";
inline constexpr char kControlLabel[] = "• Điều khiển: ";
inline constexpr char kDirectionLabel[] = "• Hướng: ";
inline constexpr char kMotionStateLabel[] = "• Hoạt động: ";
inline constexpr char kActivePwmLabel[] = "• PWM hiện tại: ";
inline constexpr char kSavedSpeedLabel[] = "• Tốc độ đã đặt: ";
inline constexpr char kEmotionLabel[] = "• Chuyển động cảm xúc: ";
inline constexpr char kHardwareLabel[] = "• Phần cứng: ";
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
inline constexpr char kSignalUnitSeparator[] = " dBm · ";
inline constexpr char kMemoryUnit[] = " KB";

inline constexpr char kUptimeFormat[] = "%lld giờ %lld phút %lld giây";
inline constexpr char kControlWeb[] = "🎮 Web thủ công";
inline constexpr char kControlVoice[] = "🎙️ Giọng nói";
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
    "<b>🤖 XIAOZHI ROBOT</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "<b>📋 DANH SÁCH LỆNH</b>\n\n"
    "🎙️ /wake — Đánh thức robot để trò chuyện\n"
    "⚙️ /speed — Xem tốc độ; /speed 50 — Đặt PWM 50%\n"
    "🔎 /status — Xem trạng thái robot\n"
    "🌐 /web — Mở trang điều khiển xe\n"
    "📖 /menu — Hiển thị menu này\n\n"
    "<i>Sau khi đánh thức, hãy nói vào mic trên robot. Điều khiển xe thực hiện trên trang web "
    "hoặc bằng giọng nói.</i>";

inline constexpr char kWakeRequested[] =
    "🎙️ Đã gửi yêu cầu đánh thức. Chờ robot chuyển sang lắng nghe rồi nói vào mic trên robot.\n"
    "Gửi /status để kiểm tra nếu robot chưa phản hồi.";
inline constexpr char kWakeAlreadyListening[] =
    "👂 Robot đang lắng nghe. Bạn có thể nói vào mic trên robot.";
inline constexpr char kWakeConnecting[] =
    "⏳ Robot đang kết nối máy chủ. Hãy chờ robot chuyển sang lắng nghe.";
inline constexpr char kWakeSpeaking[] =
    "🗣️ Robot đang nói. Hãy chờ robot nói xong và chuyển sang lắng nghe.";
inline constexpr char kWakeUnavailable[] =
    "⚠️ Robot chưa sẵn sàng để đánh thức. Gửi /status để xem trạng thái và thử lại khi robot sẵn "
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

inline constexpr char kWebReadyHeader[] =
    "<b>🌐 ĐIỀU KHIỂN XE</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "✅ Trang điều khiển đã sẵn sàng.\n\n";
inline constexpr char kWebReadyFooter[] =
    "\n\n<i>Thiết bị mở link cần truy cập được cùng mạng Wi-Fi hoặc hotspot với robot.</i>";
inline constexpr char kWebWifiDisconnected[] =
    "<b>🌐 ĐIỀU KHIỂN XE</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "❌ Robot đã mất kết nối Wi-Fi.\n\n"
    "Hãy chờ robot kết nối lại rồi gửi /web lần nữa.";
inline constexpr char kWebServerUnavailable[] =
    "<b>🌐 ĐIỀU KHIỂN XE</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "⚠️ Wi-Fi đã kết nối nhưng web server chưa sẵn sàng.\n\n"
    "Vui lòng thử lại sau vài giây hoặc kiểm tra log <code>L298nWeb</code>.";

inline constexpr char kBootHeader[] =
    "<b>🚀 XIAOZHI ĐÃ SẴN SÀNG</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "✅ Robot đã khởi động và kết nối Wi-Fi.\n";
inline constexpr char kIpChangedHeader[] =
    "<b>🔄 ĐỊA CHỈ ROBOT ĐÃ THAY ĐỔI</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "✅ Robot đã kết nối lại mạng với địa chỉ mới.\n";
inline constexpr char kBootFooter[] =
    "\n\nGửi /status để xem tình trạng robot hoặc /menu để xem các lệnh.";

inline constexpr char kUnknownCommand[] =
    "<b>❓ LỆNH CHƯA ĐƯỢC HỖ TRỢ</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "Mình chưa hiểu lệnh này. Gửi /menu để xem danh sách lệnh hiện có.";

// Telegram reply keyboard. Keep this as valid JSON.
inline constexpr char kKeyboardJson[] =
    R"({"keyboard":[["/wake","/speed"],["/speed 25","/speed 50","/speed 75"],["/status","/web"],["/menu"]],"resize_keyboard":true})";

}  // namespace L298nTelegramText
