#include "controller_internal.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "imu.h"

namespace ruby_robot {

DistanceDriveLimits MotorController::DistanceLimits() const {
    double metres_per_second_per_percent = 0;
    if (config_.encoder.enabled) {
        // Heading PD requests a common RPM. Convert that RPM to centre speed using
        // the mean calibrated wheel circumference, exactly as the two wheel distances.
        metres_per_second_per_percent =
            std::min(config_.speed_pi_left_reference_rpm, config_.speed_pi_right_reference_rpm) *
            std::numbers::pi *
            (config_.encoder.wheel_diameter_mm[0] + config_.encoder.wheel_diameter_mm[1]) * 0.5 /
            (1000.0 * 60.0 * config_.speed_pi_reference_pwm_percent);
    }
    return {metres_per_second_per_percent,
            config_.distance_drive_acceleration_mps2,
            config_.distance_drive_deceleration_mps2,
            config_.distance_drive_tolerance_mm / 1000.0,
            config_.distance_drive_min_speed_percent,
            config_.distance_drive_max_speed_percent,
            config_.distance_drive_settle_rpm,
            config_.distance_drive_settle_ms * 1000LL};
}

OdometrySample MotorController::ReadOdometrySampleLocked(int64_t now_us) {
    encoder_.Update(now_us);
    const auto encoder = encoder_.Snapshot();
    const auto imu = GetMpu6050Status();
    return {encoder.left.signed_distance_m,
            encoder.right.signed_distance_m,
            imu.heading_degrees,
            encoder.left.unknown_direction_pulses,
            encoder.right.unknown_direction_pulses,
            now_us,
            encoder.ready && encoder.calibrated && imu.ready && imu.heading_valid};
}

bool MotorController::OdometryStationaryLocked(int64_t now_us) const {
    const auto encoder = encoder_.Snapshot();
    const auto imu = GetMpu6050Status();
    // Wait for commanded coasting before changing the pose origin.
    const int64_t stationary_reset_us = std::max<int64_t>(
        500000, config_.encoder.enabled ? config_.encoder.coast_ms * 1000LL : 0);
    return motion_ == Motion::kStop && !moving_ &&
           now_us - last_motor_stop_us_ >= stationary_reset_us && encoder.ready &&
           encoder.calibrated && encoder.left.rpm <= config_.distance_drive_settle_rpm &&
           encoder.right.rpm <= config_.distance_drive_settle_rpm && imu.ready && imu.heading_valid &&
           std::abs(imu.gyro_z_dps) <= config_.heading_turn_settle_rate_dps;
}

void MotorController::UpdateOdometryLocked(int64_t now_us) {
    const auto sample = ReadOdometrySampleLocked(now_us);
    if (!odometry_.Snapshot().initialized && OdometryStationaryLocked(now_us)) {
        odometry_.Reset(sample, config_.odometry_track_width_mm / 1000.0);
        ekf_.Reset(sample, config_.odometry_track_width_mm / 1000.0);
    } else {
        odometry_.Update(sample);
        ekf_.Update(sample);
    }
}

OdometrySnapshot MotorController::NavigationPoseLocked() const {
    auto pose = odometry_.Snapshot();
    if (config_.navigation_use_ekf) {
        const auto& ekf = ekf_.Snapshot();
        if (!ekf.valid || ekf.variance_x_m2 + ekf.variance_y_m2 > 0.09 ||
            ekf.variance_theta_rad2 > 0.12) {
            pose.valid = false;
            pose.fault = ekf.valid ? "ekf_uncertain" : ekf.fault;
        } else {
            pose.x_m = ekf.x_m;
            pose.y_m = ekf.y_m;
            pose.theta_rad = ekf.theta_rad;
        }
    }
    return pose;
}

esp_err_t MotorController::ResetOdometry() {
    std::lock_guard lock(mutex_);
    const int64_t now_us = esp_timer_get_time();
    const auto sample = ReadOdometrySampleLocked(now_us);
    if (command_interface_.mode() != CommandMode::kIdle || web_control_.active() ||
        !OdometryStationaryLocked(now_us))
        return ESP_ERR_INVALID_STATE;
    if (!odometry_.Reset(sample, config_.odometry_track_width_mm / 1000.0))
        return ESP_ERR_INVALID_STATE;
    ekf_.Reset(sample, config_.odometry_track_width_mm / 1000.0);
    return ESP_OK;
}

}  // namespace ruby_robot
