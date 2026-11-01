#pragma once

// Board-lifetime task. Initialize once after the motor controller, before networking.
// The board supplies web readiness so Telegram does not depend on the web adapter.
// No task is allocated until Telegram has been explicitly configured locally.
void InitializeL298nTelegram(bool (*web_server_ready)());
// Callback only records connectivity and wakes the task; never performs HTTPS here.
void UpdateL298nTelegramNetwork(bool connected);
// Non-blocking notification to the configured owner's chat; false if unavailable/full.
