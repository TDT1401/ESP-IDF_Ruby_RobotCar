#pragma once

#include <esp_err.h>

#include <cstdint>
#include <functional>
#include <variant>

#include "commands/command_interface.h"
#include "domain/pose_ekf.h"
#include "imu.h"
#include "motor_driver.h"
#include "ultrasonic_sensor.h"
#include "wheel_encoder.h"

namespace ruby_robot {

struct MotorControllerConfig {
    MotorDriverConfig motor;
    WheelEncoderConfig encoder;
    ImuConfig imu;
    UltrasonicSensorConfig ultrasonic;

    int speed_pi_reference_pwm_percent;
    double speed_pi_left_reference_rpm;
    double speed_pi_right_reference_rpm;
    double speed_pi_left_kp;
    double speed_pi_left_ki;
    double speed_pi_right_kp;
    double speed_pi_right_ki;
    double heading_pd_kp_rpm_per_deg;
    double heading_pd_kd_rpm_per_dps;
    double heading_pd_max_correction_rpm;
    double heading_turn_max_rpm;
    double heading_turn_kp_rpm_per_deg;
    double heading_turn_acceleration_rpm_per_s;
    double heading_turn_deceleration_rpm_per_s;
    double heading_turn_min_rpm;
    double heading_turn_tolerance_degrees;
    double heading_turn_settle_rate_dps;
    double heading_turn_settle_rpm;
    double heading_turn_left_friction_pwm_percent;
    double heading_turn_right_friction_pwm_percent;
    double odometry_track_width_mm;
    bool navigation_use_ekf;
    int distance_drive_max_speed_percent;
    double distance_drive_acceleration_mps2;
    double distance_drive_deceleration_mps2;
    int distance_drive_min_speed_percent;
    double distance_drive_left_friction_pwm_percent;
    double distance_drive_right_friction_pwm_percent;
    double distance_drive_tolerance_mm;
    double distance_drive_settle_rpm;
    int distance_drive_settle_ms;

    // Supplied by the board; callbacks must remain valid for the firmware lifetime.
    bool (*motion_allowed)();
    void (*schedule)(std::function<void()> callback);
    void (*play_proximity_warning)();
};

}  // namespace ruby_robot

// Board-lifetime controller; initialize before calling the runtime API below.
void InitializeL298nController(const ruby_robot::MotorControllerConfig& config);

struct L298nWebDriveRequest {
    uint32_t session;
    uint32_t sequence;
    int speed_percent;
    bool proximity_warning_enabled;
};

using L298nLinearRequest =
    std::variant<ruby_robot::DistanceDriveRequest, L298nWebDriveRequest>;
using L298nTurnRequest = std::variant<ruby_robot::AngleTurnRequest, L298nWebDriveRequest>;

// Thread-safe board-local API. A single browser owns a renewable 600 ms session.
// Expiry or network loss stops motors and invalidates the session.
esp_err_t BeginL298nWebControl(uint32_t* session);
esp_err_t ReleaseL298nWebControl(const L298nWebDriveRequest& request);
void SetL298nWebConnected(bool connected);

// Start a command through the shared controller safety and motor path.
esp_err_t StartL298nCommand(const ruby_robot::CommandRequest& request);

// Five movement operations. A goal request is asynchronous; observe its result
// through GetL298nMotorStatus(). A web request keeps the manual session alive.
// Positive magnitudes: distance 5-1000 cm, angle 5-180 degrees. Distance and
// angle goals use automatic speed profiles; callers do not supply a speed.
esp_err_t MoveL298nForward(const L298nLinearRequest& request);
esp_err_t MoveL298nBackward(const L298nLinearRequest& request);
esp_err_t TurnL298nLeft(const L298nTurnRequest& request);
esp_err_t TurnL298nRight(const L298nTurnRequest& request);
// With a web request, stop the wheels and retain its session. Without one,
// stop immediately and revoke web control.
esp_err_t StopL298nMotor(const L298nWebDriveRequest* request = nullptr);

// Establish (0,0,0) while idle and stationary, without resetting sensor calibration.
esp_err_t ResetL298nOdometry();

// Read-only snapshot. Speed PI is available after wheel RPM references are configured.
struct L298nMotorStatus {
    ruby_robot::CommandSnapshot command;
    const char* motion;
    int speed_percent;
    int active_speed_percent;
    bool moving;
    bool pending;
    bool web_manual;
    int obstacle_distance_cm;
    int obstacle_stop_effective_cm;
    int web_warning_effective_cm;
    int measured_distance_cm;
    bool distance_valid;
    esp_err_t hardware_error;
    Fc03EncoderStatus encoder;
    ruby_robot::OdometrySnapshot odometry;
    ruby_robot::EkfSnapshot ekf;
    const char* distance_move_state;
    int distance_move_target_cm;
    double distance_move_progress_cm;
    int turn_target_degrees;
    double turn_progress_degrees;
    bool imu_ready;
    esp_err_t imu_error;
    bool imu_axis_mismatch;
    bool heading_valid;
    double heading_degrees;
    double gyro_z_dps;
    bool heading_hold_active;
    double heading_target_degrees;
    double heading_error_degrees;
    double heading_correction_rpm;
    const char* heading_fault;
    bool turn_uses_imu;
    bool speed_pi_calibrated;
    bool speed_pi_active;
    double speed_pi_target_rpm;
    double speed_pi_left_target_rpm;
    double speed_pi_right_target_rpm;
    const char* speed_pi_fault;
    int left_pwm_percent;
    int right_pwm_percent;
    int rear_left_pwm_percent;
    int rear_right_pwm_percent;
    const char* control_owner;
    bool warning_sound_enabled;
};
L298nMotorStatus GetL298nMotorStatus();
