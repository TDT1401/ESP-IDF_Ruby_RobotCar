#include "controller_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>

#include "imu.h"

namespace ruby_robot {
namespace {

double TargetRpm(int requested_percent, double reference_rpm, int reference_pwm_percent) {
    return std::abs(requested_percent) * reference_rpm / reference_pwm_percent;
}

int PercentForRpm(double signed_rpm, double reference_rpm, int reference_pwm_percent) {
    const double scale = reference_pwm_percent / reference_rpm;
    return std::clamp(static_cast<int>(std::lround(signed_rpm * scale)), -100, 100);
}

// The speed target remains unchanged. Only the initial PWM estimate compensates
// for friction; the PI can still lower it to zero when the wheel is too fast.
double FeedforwardPercent(int requested_percent, double friction_pwm_percent) {
    if (requested_percent == 0)
        return 0;
    return std::clamp(
        std::max(static_cast<double>(std::abs(requested_percent)), friction_pwm_percent), 0.0,
        100.0);
}

double FrictionPwmPercent(CommandMode mode, bool heading_turn, bool left,
                         int requested_percent, const MotorControllerConfig& config) {
    if (heading_turn) {
        const double friction = left ? config.heading_turn_left_friction_pwm_percent
                                     : config.heading_turn_right_friction_pwm_percent;
        const double reference_rpm = left ? config.speed_pi_left_reference_rpm
                                         : config.speed_pi_right_reference_rpm;
        const double target_rpm =
            TargetRpm(requested_percent, reference_rpm, config.speed_pi_reference_pwm_percent);
        // Ramp friction compensation with the RPM target during startup.
        return friction * std::min(1.0, target_rpm / config.heading_turn_min_rpm);
    }
    if (mode == CommandMode::kDistanceDrive || mode == CommandMode::kNavigate)
        return left ? config.distance_drive_left_friction_pwm_percent
                    : config.distance_drive_right_friction_pwm_percent;
    return 0;
}

}  // namespace

bool MotorController::SpeedPiConfiguredLocked() const {
    return config_.encoder.enabled && config_.speed_pi_left_reference_rpm > 0 &&
           config_.speed_pi_right_reference_rpm > 0;
}

bool MotorController::SpeedPiReadyLocked() const {
    const auto encoder = encoder_.Snapshot();
    return SpeedPiConfiguredLocked() && encoder.ready && encoder.calibrated;
}

bool MotorController::SetRequestedWheelsLocked(int left_percent, int right_percent) {
    const bool pi_configured = SpeedPiConfiguredLocked();
    if (pi_configured && !SpeedPiReadyLocked()) {
        speed_pi_fault_ = "encoder_error";
        return false;
    }
    if (left_percent == requested_left_percent_ && right_percent == requested_right_percent_)
        return true;
    if ((left_percent > 0) != (requested_left_percent_ > 0) ||
        (left_percent < 0) != (requested_left_percent_ < 0))
        left_speed_pi_.Reset();
    if ((right_percent > 0) != (requested_right_percent_ > 0) ||
        (right_percent < 0) != (requested_right_percent_ < 0))
        right_speed_pi_.Reset();
    const double left_friction =
        FrictionPwmPercent(mode_, heading_turn_active_, true, left_percent,
                           config_);
    const double right_friction =
        FrictionPwmPercent(mode_, heading_turn_active_, false, right_percent,
                           config_);
    int left_output =
        static_cast<int>(std::lround(FeedforwardPercent(left_percent, left_friction)));
    int right_output =
        static_cast<int>(std::lround(FeedforwardPercent(right_percent, right_friction)));
    if (pi_configured) {
        if (left_percent != 0 && requested_left_percent_ != 0)
            left_output = applied_left_pwm_percent_;
        if (right_percent != 0 && requested_right_percent_ != 0)
            right_output = applied_right_pwm_percent_;
    }
    if (!ApplyMotorPwmLocked(left_output, right_output))
        return false;
    requested_left_percent_ = left_percent;
    requested_right_percent_ = right_percent;
    if (last_speed_pi_us_ == 0)
        last_speed_pi_us_ = esp_timer_get_time();
    speed_pi_fault_ = "none";
    return true;
}

bool MotorController::UpdateTurnTargetsLocked(const Mpu6050Status& imu, int64_t now_us,
                                              HeadingPdTargets& targets) {
    const double error = heading_target_degrees_ - imu.heading_degrees;
    turn_progress_degrees_ = imu.heading_degrees - heading_turn_start_degrees_;
    const auto encoder = encoder_.Snapshot();
    const bool stationary = encoder.left.rpm <= config_.heading_turn_settle_rpm &&
                            encoder.right.rpm <= config_.heading_turn_settle_rpm &&
                            std::abs(imu.gyro_z_dps) <= config_.heading_turn_settle_rate_dps;
    const bool navigation_turn = IsNavigationTurnLocked();
    const double tolerance_degrees = navigation_turn
                                         ? Navigator::kHeadingToleranceRad * 180.0 / std::numbers::pi
                                         : config_.heading_turn_tolerance_degrees;
    const bool at_target = std::abs(error) <= tolerance_degrees;
    heading_turn_complete_ = false;
    if (navigation_turn && at_target)
        navigation_turn_stopping_ = true;
    if (navigation_turn && navigation_turn_stopping_) {
        // Keep zero targets while coasting; allow small heading drift before driving.
        if (stationary) {
            if (turn_settled_since_us_ == 0)
                turn_settled_since_us_ = now_us;
            if (now_us - turn_settled_since_us_ >= kTurnSettleUs) {
                if (std::abs(error) <= 2.0 * tolerance_degrees) {
                    heading_turn_complete_ = true;
                } else {
                    navigation_turn_stopping_ = false;
                    turn_settled_since_us_ = 0;
                }
            }
        } else {
            turn_settled_since_us_ = 0;
        }
        return true;
    }
    if (at_target && stationary) {
        if (turn_settled_since_us_ == 0)
            turn_settled_since_us_ = now_us;
        heading_turn_complete_ = now_us - turn_settled_since_us_ >= kTurnSettleUs;
    } else {
        turn_settled_since_us_ = 0;
    }
    if (at_target)
        return true;

    const int turn_sign = error > 0 ? 1 : -1;
    const int current_sign = motion_ == Motion::kLeft ? 1 : -1;
    if (turn_sign != current_sign || turn_reverse_until_us_ != 0) {
        if (turn_reverse_until_us_ == 0) {
            // Cut PWM before changing direction; keep coast pulses on the old sign.
            if (!SetRequestedWheelsLocked(0, 0))
                return false;
            encoder_.SetDrive(0, 0, now_us);
            if (!ApplyWheelDirectionsLocked(0, 0))
                return false;
            turn_reverse_until_us_ =
                now_us + std::max(kDirectionDelayUs, config_.encoder.coast_ms * int64_t{1000});
            turn_reverse_settled_since_us_ = 0;
        }
        if (!stationary) {
            turn_reverse_settled_since_us_ = 0;
            return true;
        }
        if (turn_reverse_settled_since_us_ == 0)
            turn_reverse_settled_since_us_ = now_us;
        if (now_us < turn_reverse_until_us_ ||
            now_us - turn_reverse_settled_since_us_ < kTurnSettleUs)
            return true;
        if (!ApplyWheelDirectionsLocked(-turn_sign, turn_sign))
            return false;
        motion_ = turn_sign > 0 ? Motion::kLeft : Motion::kRight;
        turn_reverse_until_us_ = 0;
        turn_reverse_settled_since_us_ = 0;
        // Restart the profile from zero after the direction pins have been set.
        return true;
    }

    const double reference_rpm =
        std::min(config_.speed_pi_left_reference_rpm, config_.speed_pi_right_reference_rpm);
    const double maximum_rpm =
        std::min(config_.heading_turn_max_rpm,
                 reference_rpm * 100.0 / config_.speed_pi_reference_pwm_percent);
    const auto turn = HeadingPd::Update(0, heading_target_degrees_, imu.heading_degrees,
                                        imu.gyro_z_dps, config_.heading_turn_kp_rpm_per_deg,
                                        config_.heading_pd_kd_rpm_per_dps, maximum_rpm);
    const double forward_rpm = turn_sign * turn.correction_rpm;
    const double desired_rpm =
        forward_rpm > 0
            ? std::clamp(forward_rpm, std::min(config_.heading_turn_min_rpm, maximum_rpm),
                         maximum_rpm)
            : 0;
    const double dt_s = last_heading_pd_us_ == 0
                            ? kHeadingPdPeriodUs / 1000000.0
                            : std::clamp((now_us - last_heading_pd_us_) / 1000000.0, 0.0, 0.1);
    const double previous_rpm = std::abs(heading_correction_rpm_);
    const double rpm = previous_rpm +
        std::clamp(desired_rpm - previous_rpm, -config_.heading_turn_deceleration_rpm_per_s * dt_s,
                   config_.heading_turn_acceleration_rpm_per_s * dt_s);
    targets.correction_rpm = turn_sign * rpm;
    targets.left_rpm = -targets.correction_rpm;
    targets.right_rpm = targets.correction_rpm;
    return true;
}

bool MotorController::UpdateHeadingPdLocked(int64_t now_us) {
    if (!heading_hold_active_)
        return true;
    const auto imu = GetMpu6050Status();
    if (!imu.ready || !imu.heading_valid) {
        heading_fault_ = !imu.ready ? "imu_unavailable" : "heading_invalid";
        return false;
    }
    if (IsNavigationLocked()) {
        heading_target_degrees_ =
            imu.heading_degrees + navigator_.heading_error_rad() * 180.0 / std::numbers::pi;
    }
    const int speed = ActiveSpeedLocked();
    // A distance goal reaching its braking point must zero the targets immediately.
    if (last_heading_pd_us_ != 0 && now_us - last_heading_pd_us_ < kHeadingPdPeriodUs &&
        (speed != 0 || (requested_left_percent_ == 0 && requested_right_percent_ == 0)))
        return true;

    // Use a common wheel RPM even if the two sides have different reference RPM.
    const double reference_rpm =
        std::min(config_.speed_pi_left_reference_rpm, config_.speed_pi_right_reference_rpm);
    const double base_rpm = reference_rpm * speed / config_.speed_pi_reference_pwm_percent;
    const double error = heading_target_degrees_ - imu.heading_degrees;
    HeadingPdTargets targets{};
    if (heading_turn_active_) {
        if (!UpdateTurnTargetsLocked(imu, now_us, targets)) {
            heading_fault_ = fault_ != ESP_OK ? "motor_error" : speed_pi_fault_;
            return false;
        }
    } else {
        const double signed_rpm = motion_ == Motion::kBackward ? -base_rpm : base_rpm;
        const double correction_limit =
            std::min(config_.heading_pd_max_correction_rpm, base_rpm * 0.5);
        targets = HeadingPd::Update(signed_rpm, heading_target_degrees_, imu.heading_degrees,
                                    imu.gyro_z_dps, config_.heading_pd_kp_rpm_per_deg,
                                    config_.heading_pd_kd_rpm_per_dps, correction_limit);
    }
    const int left_percent = PercentForRpm(targets.left_rpm, config_.speed_pi_left_reference_rpm,
                                          config_.speed_pi_reference_pwm_percent);
    const int right_percent = PercentForRpm(targets.right_rpm, config_.speed_pi_right_reference_rpm,
                                           config_.speed_pi_reference_pwm_percent);
    if (!SetRequestedWheelsLocked(left_percent, right_percent)) {
        heading_fault_ = fault_ != ESP_OK ? "motor_error" : speed_pi_fault_;
        return false;
    }
    encoder_.SetDrive((left_percent > 0) - (left_percent < 0),
                      (right_percent > 0) - (right_percent < 0), now_us);
    heading_error_degrees_ = error;
    heading_correction_rpm_ = targets.correction_rpm;
    last_heading_pd_us_ = now_us;
    heading_fault_ = "none";
    return true;
}

bool MotorController::UpdateSpeedPiLocked(int64_t now_us) {
    if (!SpeedPiConfiguredLocked())
        return true;
    const auto encoder = encoder_.Snapshot();
    if (!encoder.ready || !encoder.calibrated) {
        speed_pi_fault_ = "encoder_error";
        return false;
    }
    if ((requested_left_percent_ != 0 && encoder.left.no_pulses) ||
        (requested_right_percent_ != 0 && encoder.right.no_pulses)) {
        speed_pi_fault_ = "no_pulses";
        return false;
    }
    if (last_speed_pi_us_ != 0 && now_us - last_speed_pi_us_ < kSpeedPiPeriodUs)
        return true;
    const double dt_s = last_speed_pi_us_ == 0 ? kSpeedPiPeriodUs / 1000000.0
                                               : (now_us - last_speed_pi_us_) / 1000000.0;
    if (dt_s > 0.5) {
        left_speed_pi_.Reset();
        right_speed_pi_.Reset();
    }
    const double bounded_dt_s = std::min(dt_s, 0.25);
    const double left_target_rpm =
        TargetRpm(requested_left_percent_, config_.speed_pi_left_reference_rpm,
                  config_.speed_pi_reference_pwm_percent);
    const double right_target_rpm =
        TargetRpm(requested_right_percent_, config_.speed_pi_right_reference_rpm,
                  config_.speed_pi_reference_pwm_percent);
    const double left_feedforward = FeedforwardPercent(
        requested_left_percent_,
        FrictionPwmPercent(mode_, heading_turn_active_, true,
                           requested_left_percent_, config_));
    const double right_feedforward = FeedforwardPercent(
        requested_right_percent_,
        FrictionPwmPercent(mode_, heading_turn_active_, false,
                           requested_right_percent_, config_));
    const int left =
        requested_left_percent_ == 0
            ? 0
            : left_speed_pi_.Update(left_target_rpm, encoder.left.rpm, left_feedforward,
                                    bounded_dt_s, config_.speed_pi_left_kp, config_.speed_pi_left_ki);
    const int right =
        requested_right_percent_ == 0
            ? 0
            : right_speed_pi_.Update(right_target_rpm, encoder.right.rpm, right_feedforward,
                                     bounded_dt_s, config_.speed_pi_right_kp, config_.speed_pi_right_ki);
    last_speed_pi_us_ = now_us;
    if (!ApplyMotorPwmLocked(left, right))
        return false;
    return true;
}

}  // namespace ruby_robot
