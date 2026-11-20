#pragma once

#include <cstddef>
#include <expected>
#include <string>

namespace ruby_robot {

inline constexpr std::size_t kTelegramMaxOutboundText = 4000;

struct TelegramConfig {
    std::string bot_token;
    std::string chat_id;
    // Called on the Telegram task; must remain valid for the firmware lifetime.
    bool (*can_send)() = nullptr;
};

}  // namespace ruby_robot

// Initialize once before networking. Configuration strings are copied internally.
bool InitializeTelegram(const ruby_robot::TelegramConfig& config);
// Only records connectivity and wakes the task; never performs HTTPS here.
void UpdateTelegramNetwork(bool connected);
// Nonblocking; success confirms queueing, not delivery.
std::expected<std::string, std::string> QueueTelegramMessage(const std::string& text);
