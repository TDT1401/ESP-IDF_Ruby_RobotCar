#pragma once

// Register the board-local MCP adapter after the motor controller is ready.
void RegisterL298nMcpTools();

// Register after Telegram initialization succeeds.
void RegisterTelegramMcpTools();
