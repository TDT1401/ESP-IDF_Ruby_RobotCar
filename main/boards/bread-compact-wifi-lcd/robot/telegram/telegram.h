#pragma once

// Board-lifetime task. Initialize once before networking.
// No task is allocated until Telegram has been explicitly configured locally.
void InitializeL298nTelegram();
// Callback only records connectivity and wakes the task; never performs HTTPS here.
void UpdateL298nTelegramNetwork(bool connected);
