#include "controller_internal.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../config.h"
#include "../sensors/mpu6050_imu.h"

namespace ruby_robot {
namespace {
// Allow commanded coasting to finish before changing the pose origin or starting
// a measured move. An explicit reset never erases encoder counters or gyro bias.
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
constexpr int64_t kStationaryResetUs = std::max<int64_t>(500000, FC03_COAST_MS * 1000LL);
#else
constexpr int64_t kStationaryResetUs = 500000;
#endif
static_assert(ODOMETRY_TRACK_WIDTH_MM >= 0);
static_assert(DISTANCE_DRIVE_ACCELERATION_MPS2 > 0 && DISTANCE_DRIVE_DECELERATION_MPS2 > 0);
static_assert(DISTANCE_DRIVE_MAX_SPEED_PERCENT >= DISTANCE_DRIVE_MIN_SPEED_PERCENT &&
              DISTANCE_DRIVE_MAX_SPEED_PERCENT <= 100);
static_assert(DISTANCE_DRIVE_MIN_SPEED_PERCENT > 0 && DISTANCE_DRIVE_MIN_SPEED_PERCENT <= 100);
static_assert(DISTANCE_DRIVE_TOLERANCE_MM > 0 &&
              DISTANCE_DRIVE_TOLERANCE_MM < DistanceDriveCommand::kMinDistanceCm * 10);
static_assert(DISTANCE_DRIVE_SETTLE_RPM >= 0 && DISTANCE_DRIVE_SETTLE_MS > 0);
}  // namespace

DistanceDriveLimits L298nController::DistanceLimits() {
    double metres_per_second_per_percent = 0;
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
    // Heading PD requests a common RPM. Convert that RPM to centre speed using
    // the mean calibrated wheel circumference, exactly as the two wheel distances.
    metres_per_second_per_percent =
        std::min(SPEED_PI_LEFT_REFERENCE_RPM, SPEED_PI_RIGHT_REFERENCE_RPM) * std::numbers::pi *
        (FC03_LEFT_WHEEL_DIAMETER_MM + FC03_RIGHT_WHEEL_DIAMETER_MM) * 0.5 /
        (1000.0 * 60.0 * SPEED_PI_REFERENCE_PWM_PERCENT);
#endif
    return {metres_per_second_per_percent,    DISTANCE_DRIVE_ACCELERATION_MPS2,
            DISTANCE_DRIVE_DECELERATION_MPS2, DISTANCE_DRIVE_TOLERANCE_MM / 1000.0,
            DISTANCE_DRIVE_MIN_SPEED_PERCENT, DISTANCE_DRIVE_SETTLE_RPM,
            DISTANCE_DRIVE_SETTLE_MS * 1000LL};
}

OdometrySample L298nController::ReadOdometrySampleLocked(int64_t now_us) {
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

bool L298nController::OdometryStationaryLocked(int64_t now_us) const {
    const auto encoder = encoder_.Snapshot();
    const auto imu = GetMpu6050Status();
    return motion_ == Motion::kStop && !moving_ && !web_joystick_pending_ &&
           now_us - last_motor_stop_us_ >= kStationaryResetUs && encoder.ready &&
           encoder.calibrated && encoder.left.rpm <= DISTANCE_DRIVE_SETTLE_RPM &&
           encoder.right.rpm <= DISTANCE_DRIVE_SETTLE_RPM && imu.ready && imu.heading_valid &&
           std::abs(imu.gyro_z_dps) <= HEADING_TURN_SETTLE_RATE_DPS;
}

void L298nController::UpdateOdometryLocked(int64_t now_us) {
    const auto sample = ReadOdometrySampleLocked(now_us);
    if (!odometry_.Snapshot().initialized && OdometryStationaryLocked(now_us))
        odometry_.Reset(sample, ODOMETRY_TRACK_WIDTH_MM / 1000.0);
    else
        odometry_.Update(sample);
}

esp_err_t L298nController::ResetOdometry() {
    std::lock_guard lock(mutex_);
    const int64_t now_us = esp_timer_get_time();
    const auto sample = ReadOdometrySampleLocked(now_us);
    if (command_interface_.mode() != CommandMode::kIdle || web_control_.active() ||
        !OdometryStationaryLocked(now_us))
        return ESP_ERR_INVALID_STATE;
    return odometry_.Reset(sample, ODOMETRY_TRACK_WIDTH_MM / 1000.0) ? ESP_OK
                                                                     : ESP_ERR_INVALID_STATE;
}

}  // namespace ruby_robot
