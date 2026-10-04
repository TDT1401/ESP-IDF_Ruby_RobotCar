#pragma once

#include <esp_err.h>

#include <cstdint>

// Board-lifetime controller; call once after the LCD backlight is initialized.
void InitializeL298nController();

// Called only after initialization. Records display events; motion runs on the motor timer.
void UpdateL298nEmotion(const char* emotion, bool speaking);

enum class L298nWebCommand { kStop, kForward, kBackward, kLeft, kRight, kRelease };

// Thread-safe board-local API. A single browser owns a renewable 600 ms session.
// Expiry stops motors and invalidates the session; manual mode stays latched until release.
esp_err_t BeginL298nWebControl(uint32_t* session);
esp_err_t CommandL298nWebControl(uint32_t session, uint32_t sequence, L298nWebCommand command,
                                 int speed_percent);
void SetL298nWebConnected(bool connected);
