#pragma once

// Board-lifetime task. Initialize once after the motor controller, before networking.
// No task is allocated until Telegram has been explicitly configured locally.
void InitializeL298nTelegram();
// Callback only records connectivity and wakes the task; never performs HTTPS here.
void UpdateL298nTelegramNetwork(bool connected);
// Non-blocking notification to the configured owner's chat; false if unavailable/full.
bool QueueL298nTelegramAlert(const char* text);
