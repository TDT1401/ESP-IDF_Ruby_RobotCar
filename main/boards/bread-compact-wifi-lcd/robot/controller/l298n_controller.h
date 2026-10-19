#pragma once

#include <esp_err.h>

#include <array>
#include <cstdint>

#include "../sensors/fc03_encoder.h"

// Board-lifetime controller; call once after the LCD backlight is initialized.
void InitializeL298nController();

// Called only after initialization. Records display events; motion runs on the motor timer.
void UpdateL298nEmotion(const char* emotion, bool speaking);

enum class L298nWebCommand { kStop, kForward, kBackward, kLeft, kRight, kJoystick, kRelease };

// Thread-safe board-local API. A single browser owns a renewable 600 ms session.
// Expiry or network loss stops motors, invalidates the session and returns control to voice.
esp_err_t BeginL298nWebControl(uint32_t* session);
esp_err_t CommandL298nWebControl(uint32_t session, uint32_t sequence, L298nWebCommand command,
                                 int speed_percent, bool proximity_warning_enabled,
                                 int joystick_x = 0, int joystick_y = 0);
void SetL298nWebConnected(bool connected);

// Thread-safe shared voice/Telegram speed setting (RAM only, 0..100 PWM percent).
// Applies to current voice motion without extending its deadline; never starts motion.
// Zero stops. Returns ESP_ERR_INVALID_STATE while web manual control owns the motors.
// ESP_ERR_INVALID_ARG rejects out-of-range values; ESP_FAIL reports a hardware fault.
esp_err_t SetL298nMotorSpeed(int speed_percent);

// Start an encoder-controlled straight-line move (FC-03 builds only).
// Distance is 1..1000 cm; forward movement also observes the obstacle stop threshold.
esp_err_t MoveL298nDistance(bool forward, int distance_cm);

// Thread-safe HC-SR04 autonomous modes. Modes are mutually exclusive and RAM-only.
// Disabling either active mode stops the motors. Manual/web movement also disables autonomy.
esp_err_t SetL298nFollowMode(bool enabled);
esp_err_t SetL298nObstacleAvoidance(bool enabled);
esp_err_t SetL298nFollowDistance(int distance_cm);
esp_err_t SetL298nObstacleDistance(int distance_cm);
esp_err_t SetL298nGuardMode(bool enabled);
esp_err_t StartL298nSpaceScan();
void CancelL298nSpaceScan();
// One RAM-only script shared by web, Telegram and voice. F/B: 1..50 cm;
// L/R: 1..180 degrees, comma separated, optional terminal S.
// Up to five moves.
esp_err_t SaveL298nRoute(const char* script);
esp_err_t RunL298nRoute(const char* owner);
void CancelL298nRoute();
void StopL298nMotor();

// Read-only snapshot. PWM/moving describe commanded outputs; encoder is measured feedback.
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
    int follow_target_effective_cm;
    int obstacle_stop_effective_cm;
    int web_warning_effective_cm;
    int measured_distance_cm;
    bool distance_valid;
    bool autonomy_blocked;
    esp_err_t hardware_error;
    Fc03EncoderStatus encoder;
    const char* distance_move_state;
    int distance_move_target_cm;
    double distance_move_progress_cm;
    int turn_target_degrees;
    double turn_progress_degrees;
    bool speed_pi_calibrated;
    bool speed_pi_active;
    double speed_pi_target_rpm;
    int left_pwm_percent;
    int right_pwm_percent;
    bool guard_enabled;
    int guard_baseline_cm;
    const char* scan_state;
    int scan_left_cm;
    int scan_center_cm;
    int scan_right_cm;
    const char* control_owner;
    bool warning_sound_enabled;
    const char* route_state;
    std::array<char, 64> route_script;
    int route_step;
    int route_steps;
};
L298nMotorStatus GetL298nMotorStatus();
