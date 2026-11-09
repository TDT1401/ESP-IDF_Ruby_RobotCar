#pragma once

// Telegram HTML messages. Keep every opened tag closed.
namespace L298nTelegramText {

inline constexpr char kDivider[] = "━━━━━━━━━━━━━━━━";
inline constexpr char kStatusHeader[] = "<b>🤖 RUBY · TRẠNG THÁI</b>";
inline constexpr char kSystemSection[] = "<b>🧠 HỆ THỐNG</b>";
inline constexpr char kNetworkSection[] = "<b>📶 KẾT NỐI</b>";
inline constexpr char kUptimeLabel[] = "• Hoạt động: ";
inline constexpr char kFreeRamLabel[] = "• RAM trống: ";
inline constexpr char kWifiLabel[] = "• Wi-Fi: ";
inline constexpr char kSignalLabel[] = "• Tín hiệu: ";
inline constexpr char kIpLabel[] = "• IP: ";
inline constexpr char kMemoryUnit[] = " KB";
inline constexpr char kSignalUnitSeparator[] = " dBm · ";
inline constexpr char kWifiConnected[] = "✅ Đã kết nối";
inline constexpr char kWifiDisconnected[] = "❌ Đã ngắt";
inline constexpr char kRssiExcellent[] = "Rất tốt";
inline constexpr char kRssiGood[] = "Tốt";
inline constexpr char kRssiFair[] = "Trung bình";
inline constexpr char kRssiWeak[] = "Yếu";

inline constexpr char kMenu[] =
    "<b>Ruby · Trợ giúp</b>\n"
    "Dùng /wake để đánh thức Ruby, /status để xem trạng thái hệ thống và /menu để xem trợ giúp.";
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
    "⚠️ Ruby chưa sẵn sàng để đánh thức. Chạm nút 📊 Trạng thái để kiểm tra rồi thử lại khi Ruby sẵn sàng.";

inline constexpr char kBootHeader[] =
    "<b>🚀 Ruby ĐÃ SẴN SÀNG</b>\n"
    "━━━━━━━━━━━━━━━━\n";
inline constexpr char kBootFooter[] =
    "\nGửi /status để xem trạng thái hệ thống hoặc /menu để xem các lệnh.";
inline constexpr char kIpChangedHeader[] =
    "<b>🔄 ĐỊA CHỈ RUBY ĐÃ THAY ĐỔI</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "✅ Ruby đã kết nối lại mạng với địa chỉ mới.\n";
inline constexpr char kUnknownCommand[] =
    "<b>❓ LỆNH CHƯA ĐƯỢC HỖ TRỢ</b>\n"
    "━━━━━━━━━━━━━━━━\n"
    "Mình chưa hiểu lệnh này. Gửi /menu để xem danh sách lệnh hiện có.";

inline constexpr char kKeyboardJson[] =
    R"({"keyboard":[["🎙️ Đánh thức","📊 Trạng thái"],["📖 Trợ giúp"]],"resize_keyboard":true})";

}  // namespace L298nTelegramText
