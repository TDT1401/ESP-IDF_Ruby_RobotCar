#pragma once

// Edit this file to customize Telegram wording and icons.
// Messages use Telegram HTML: supported tags include <b>, <i>, <code> and <a>.
// Keep every opened tag closed. Dynamic values (IP, firmware, status) are added
// by telegram.cc and should not be written into these strings.
namespace L298nTelegramText {

inline constexpr char kDivider[] = "━━━━━━━━━━━━━━━━";

inline constexpr char kStatusHeader[] = "<b>🤖 RUBY · TRẠNG THÁI</b>";
inline constexpr char kSystemSection[] = "<b>🧠 HỆ THỐNG</b>";
inline constexpr char kDeviceSection[] = "<b>🔌 THIẾT BỊ</b>";
inline constexpr char kMotorSection[] = "<b>🚗 ĐỘNG CƠ</b>";
inline constexpr char kNavigationSection[] = "<b>📡 CẢM BIẾN &amp; TỰ HÀNH</b>";
inline constexpr char kNetworkSection[] = "<b>📶 KẾT NỐI</b>";
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
    "<b>🤖 Ruby · ĐIỀU KHIỂN XE</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "Chọn chức năng bằng các nút bên dưới.\n"
    "⬆️ Tiến / ⬇️ Lùi: chạm nút rồi nhập khoảng cách bằng cm (1–1000).\n"
    "🎙️ Đánh thức Ruby để trò chuyện; 🌐 mở trang điều khiển xe.\n"
    "📊 Trạng thái và các nút tự hành cũng có sẵn bên dưới.\n"
    "Canh gác: /guard on hoặc /guard off. Quét khoảng trống: /scan hoặc /scan stop.\n"
    "Kịch bản: /route F30,L90,S; lưu để chạy lại: /route save F30,L90,S rồi /route run.\n"
    "Tìm xe: /find để phát âm báo, nhấp nháy màn hình và hiện IP.";

inline constexpr char kWakeRequested[] =
    "🎙️ Đã gửi yêu cầu đánh thức. Chờ Ruby chuyển sang lắng nghe rồi nói vào mic trên Ruby.\n"
    "Chạm nút 📊 Trạng thái nếu Ruby chưa phản hồi.";
inline constexpr char kWakeAlreadyListening[] =
    "👂 Ruby đang lắng nghe. Bạn có thể nói vào mic trên Ruby.";
inline constexpr char kWakeConnecting[] =
    "⏳ Ruby đang kết nối máy chủ. Hãy chờ Ruby chuyển sang lắng nghe.";
inline constexpr char kWakeSpeaking[] =
    "🗣️ Ruby đang nói. Hãy chờ Ruby nói xong và chuyển sang lắng nghe.";
inline constexpr char kWakeUnavailable[] =
    "⚠️ Ruby chưa sẵn sàng để đánh thức. Chạm nút 📊 Trạng thái để kiểm tra rồi thử lại khi Ruby sẵn "
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
    "❌ Có lỗi phần cứng motor khi đặt tốc độ. Chạm nút 📊 Trạng thái để kiểm tra dây nối và khởi "
    "động lại.";

inline constexpr char kDistanceMoveUsage[] =
    "Khoảng cách phải là số nguyên từ 1 đến 1000 cm. Cần hai encoder FC-03 đã được hiệu chuẩn.";
inline constexpr char kForwardDistancePrompt[] =
    "⬆️ Nhập quãng đường muốn tiến bằng cm (1–1000). Ví dụ: <code>100</code>.";
inline constexpr char kBackwardDistancePrompt[] =
    "⬇️ Nhập quãng đường muốn lùi bằng cm (1–1000). Ví dụ: <code>100</code>.";
inline constexpr char kDistanceInputInvalid[] =
    "Hãy nhập một số nguyên từ 1 đến 1000 cm. Xe chưa bắt đầu chạy.";
inline constexpr char kDistanceInputExpired[] =
    "Yêu cầu nhập quãng đường đã hết hạn. Chạm nút Tiến hoặc Lùi để bắt đầu lại.";
inline constexpr char kDistanceMoveAccepted[] =
    "✅ Đã nhận lệnh chạy theo quãng đường encoder. Motor tự dừng khi đạt mục tiêu.";
inline constexpr char kDistanceMoveWebBusy[] =
    "🎮 Web đang giữ quyền điều khiển motor. Hãy trả quyền trên trang web rồi gửi lại lệnh.";
inline constexpr char kDistanceMoveUnavailable[] =
    "❌ Không thể chạy theo quãng đường: FC-03 chưa sẵn sàng/hiệu chuẩn, PWM đang bằng 0, hoặc motor không thể chạy. Chạm nút 📊 Trạng thái để kiểm tra.";

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
    "\n\n<i>Thiết bị mở link cần truy cập được cùng mạng Wi-Fi hoặc hotspot với Ruby.</i>";
inline constexpr char kWebWifiDisconnected[] =
    "<b>🌐 ĐIỀU KHIỂN XE</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "❌ Ruby đã mất kết nối Wi-Fi.\n\n"
    "Hãy chờ Ruby kết nối lại rồi gửi /web lần nữa.";
inline constexpr char kWebServerUnavailable[] =
    "<b>🌐 ĐIỀU KHIỂN XE</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "⚠️ Wi-Fi đã kết nối nhưng web server chưa sẵn sàng.\n\n"
    "Vui lòng thử lại sau vài giây hoặc kiểm tra log <code>L298nWeb</code>.";

inline constexpr char kBootHeader[] =
    "<b>🚀 Ruby ĐÃ SẴN SÀNG</b>\n"
    "━━━━━━━━━━━━━━━━\n";
inline constexpr char kBootFooter[] =
    "\nGửi /status để xem tình trạng XiaoMin hoặc /menu để xem các lệnh.";
inline constexpr char kIpChangedHeader[] =
    "<b>🔄 ĐỊA CHỈ Ruby ROBOT ĐÃ THAY ĐỔI</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "✅ Ruby Robot đã kết nối lại mạng với địa chỉ mới.\n";
inline constexpr char kUnknownCommand[] =
    "<b>❓ LỆNH CHƯA ĐƯỢC HỖ TRỢ</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "Mình chưa hiểu lệnh này. Gửi /menu để xem danh sách lệnh hiện có.";

// Telegram reply keyboard. Keep this as valid JSON.
inline constexpr char kKeyboardJson[] =
    R"({"keyboard":[["⬆️ Tiến theo cm","⬇️ Lùi theo cm"],["🎙️ Đánh thức","⏹️ Dừng xe"],["👣 Bật Follow","👣 Tắt Follow"],["🚧 Bật né vật cản","🚧 Tắt né vật cản"],["📊 Trạng thái","🌐 Mở trang web"],["🐢 Tốc độ 50%","🚗 Tốc độ 75%","⚡ Tốc độ 100%"],["📖 Trợ giúp"]],"resize_keyboard":true})";

}  // namespace L298nTelegramText
