#pragma once

#include <esp_err.h>

#include <cstdint>

#include "../sensors/fc03_encoder.h"

// Board-lifetime controller; call once after the LCD backlight is initialized.
void InitializeL298nController();

// Legacy board callback; an active browser drive keeps its own safety deadline.
void UpdateL298nEmotion(const char* emotion, bool speaking);

enum class L298nWebCommand { kStop, kForward, kBackward, kLeft, kRight, kJoystick, kRelease };

// Thread-safe board-local API. A single browser owns a renewable 600 ms session.
// Expiry or network loss stops motors and invalidates the session.
esp_err_t BeginL298nWebControl(uint32_t* session);
esp_err_t CommandL298nWebControl(uint32_t session, uint32_t sequence, L298nWebCommand command,
                                 int speed_percent, bool proximity_warning_enabled,
                                 int joystick_x = 0, int joystick_y = 0);
void SetL298nWebConnected(bool connected);

// Compatibility entry point: only the web control command can set manual PWM.
// Always returns ESP_ERR_NOT_SUPPORTED.
esp_err_t SetL298nMotorSpeed(int speed_percent);

// Compatibility entry point: measured-distance moves are disabled.
// Always returns ESP_ERR_NOT_SUPPORTED and never starts a motor.
esp_err_t MoveL298nDistance(bool forward, int distance_cm, const char* owner = "telegram");

// Compatibility entry points: autonomous modes and their settings are disabled.
// Always return ESP_ERR_NOT_SUPPORTED.
esp_err_t SetL298nFollowMode(bool enabled);
esp_err_t SetL298nObstacleAvoidance(bool enabled);
esp_err_t SetL298nFollowDistance(int distance_cm);
esp_err_t SetL298nObstacleDistance(int distance_cm);
void StopL298nMotor();

// Read-only snapshot. Speed PI is available after wheel RPM references are configured.
struct L298nMotorStatus {
    const char* motion;
    int speed_percent;
    int active_speed_percent;
    bool moving;
    bool pending;
    bool web_manual;
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
    bool imu_ready;
    esp_err_t imu_error;
    bool imu_axis_mismatch;
    double heading_degrees;
    double gyro_z_dps;
    bool heading_hold_active;
    bool turn_uses_imu;
    bool speed_pi_calibrated;
    bool speed_pi_active;
    double speed_pi_target_rpm;
    double speed_pi_left_target_rpm;
    double speed_pi_right_target_rpm;
    const char* speed_pi_fault;
    int left_pwm_percent;
    int right_pwm_percent;
    const char* control_owner;
    bool warning_sound_enabled;
};
L298nMotorStatus GetL298nMotorStatus();
