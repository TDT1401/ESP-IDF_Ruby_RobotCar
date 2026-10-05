#pragma once

#include <esp_err.h>

#include <cstdint>

// Board-lifetime controller; call once after the LCD backlight is initialized.
void InitializeL298nController();

// Called only after initialization. Records display events; motion runs on the motor timer.
void UpdateL298nEmotion(const char* emotion, bool speaking);

enum class L298nWebCommand { kStop, kForward, kBackward, kLeft, kRight, kRelease };

// Thread-safe board-local API. A single browser owns a renewable 600 ms session.
// Expiry or network loss stops motors, invalidates the session and returns control to voice.
esp_err_t BeginL298nWebControl(uint32_t* session);
esp_err_t CommandL298nWebControl(uint32_t session, uint32_t sequence, L298nWebCommand command,
                                 int speed_percent);
void SetL298nWebConnected(bool connected);

// Thread-safe shared voice/Telegram speed setting (RAM only, 0..100 PWM percent).
// Applies to current voice motion without extending its deadline; never starts motion.
// Zero stops. Returns ESP_ERR_INVALID_STATE while web manual control owns the motors.
// ESP_ERR_INVALID_ARG rejects out-of-range values; ESP_FAIL reports a hardware fault.
esp_err_t SetL298nMotorSpeed(int speed_percent);

// Read-only snapshot of commanded outputs, not encoder feedback.
struct L298nMotorStatus {
    const char* motion;
    int speed_percent;
    int active_speed_percent;
    bool moving;
    bool pending;
    bool web_manual;
    bool emotion_enabled;
    bool gesture_active;
    esp_err_t hardware_error;
};
L298nMotorStatus GetL298nMotorStatus();
