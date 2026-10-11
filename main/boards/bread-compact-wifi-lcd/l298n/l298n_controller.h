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

// Thread-safe HC-SR04 autonomous modes. Modes are mutually exclusive and RAM-only.
// Disabling either active mode stops the motors. Manual/web movement also disables autonomy.
esp_err_t SetL298nFollowMode(bool enabled);
esp_err_t SetL298nObstacleAvoidance(bool enabled);
esp_err_t SetL298nFollowDistance(int distance_cm);
esp_err_t SetL298nObstacleDistance(int distance_cm);
void StopL298nMotor();

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
    const char* autonomy_mode;
    int follow_distance_cm;
    int obstacle_distance_cm;
    int measured_distance_cm;
    bool distance_valid;
    bool autonomy_blocked;
    esp_err_t hardware_error;
};
L298nMotorStatus GetL298nMotorStatus();
