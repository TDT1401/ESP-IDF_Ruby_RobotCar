#pragma once

#include <esp_err.h>

// Locate a stationary robot with a short sound and a flashing IP screen.
// Safe to call from the web server or Telegram task; effects run on the main task.
esp_err_t FindL298nRobot();
