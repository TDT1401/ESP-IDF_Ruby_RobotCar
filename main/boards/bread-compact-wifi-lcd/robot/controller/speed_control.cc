#include "controller_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "../../config.h"
#include "../sensors/mpu6050_imu.h"

namespace ruby_robot {
namespace {

static_assert(SPEED_PI_REFERENCE_PWM_PERCENT > 0 && SPEED_PI_REFERENCE_PWM_PERCENT <= 100);
static_assert(SPEED_PI_LEFT_REFERENCE_RPM >= 0 && SPEED_PI_RIGHT_REFERENCE_RPM >= 0);
static_assert(SPEED_PI_LEFT_KP >= 0 && SPEED_PI_LEFT_KI >= 0);
static_assert(SPEED_PI_RIGHT_KP >= 0 && SPEED_PI_RIGHT_KI >= 0);
static_assert(HEADING_PD_KP_RPM_PER_DEG >= 0 && HEADING_PD_KD_RPM_PER_DPS >= 0);
static_assert(HEADING_PD_MAX_CORRECTION_RPM >= 0);
static_assert(HEADING_TURN_SLOWDOWN_DEGREES > HEADING_TURN_TOLERANCE_DEGREES);
static_assert(HEADING_TURN_TOLERANCE_DEGREES > 0);
static_assert(HEADING_TURN_SETTLE_RATE_DPS > 0);
static_assert(HEADING_TURN_MIN_RPM > 0);
static_assert(HEADING_TURN_LEFT_FRICTION_PWM_PERCENT >= 0 &&
              HEADING_TURN_LEFT_FRICTION_PWM_PERCENT < 100);
static_assert(HEADING_TURN_RIGHT_FRICTION_PWM_PERCENT >= 0 &&
              HEADING_TURN_RIGHT_FRICTION_PWM_PERCENT < 100);

double TargetRpm(int requested_percent, double reference_rpm) {
    return std::abs(requested_percent) * reference_rpm / SPEED_PI_REFERENCE_PWM_PERCENT;
}

int PercentForRpm(double signed_rpm, double reference_rpm) {
    const double scale = SPEED_PI_REFERENCE_PWM_PERCENT / reference_rpm;
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

}  // namespace

bool L298nController::SpeedPiConfiguredLocked() const {
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
    return SPEED_PI_LEFT_REFERENCE_RPM > 0 && SPEED_PI_RIGHT_REFERENCE_RPM > 0;
#else
    return false;
#endif
}

bool L298nController::SpeedPiReadyLocked() const {
    const auto encoder = encoder_.Snapshot();
    return SpeedPiConfiguredLocked() && encoder.ready && encoder.calibrated;
}

bool L298nController::SetRequestedWheelsLocked(int left_percent, int right_percent) {
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
    const double left_friction = heading_turn_active_ ? HEADING_TURN_LEFT_FRICTION_PWM_PERCENT : 0;
    const double right_friction =
        heading_turn_active_ ? HEADING_TURN_RIGHT_FRICTION_PWM_PERCENT : 0;
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

bool L298nController::UpdateHeadingPdLocked(int64_t now_us) {
    if (!heading_hold_active_)
        return true;
    const auto imu = GetMpu6050Status();
    if (!imu.ready || !imu.heading_valid) {
        heading_fault_ = !imu.ready ? "imu_unavailable" : "heading_invalid";
        return false;
    }
    if (last_heading_pd_us_ != 0 && now_us - last_heading_pd_us_ < kHeadingPdPeriodUs)
        return true;

    // Use a common wheel RPM even if the two sides have different reference RPM.
    const double reference_rpm =
        std::min(SPEED_PI_LEFT_REFERENCE_RPM, SPEED_PI_RIGHT_REFERENCE_RPM);
    const double base_rpm = reference_rpm * ActiveSpeedLocked() / SPEED_PI_REFERENCE_PWM_PERCENT;
    const double error = heading_target_degrees_ - imu.heading_degrees;
    HeadingPdTargets targets{};
    if (heading_turn_active_) {
        turn_progress_degrees_ = imu.heading_degrees - heading_turn_start_degrees_;
        const bool at_target = std::abs(error) <= HEADING_TURN_TOLERANCE_DEGREES;
        if (at_target && std::abs(imu.gyro_z_dps) <= HEADING_TURN_SETTLE_RATE_DPS) {
            if (turn_settled_since_us_ == 0)
                turn_settled_since_us_ = now_us;
            heading_turn_complete_ = now_us - turn_settled_since_us_ >= kTurnSettleUs;
        } else {
            turn_settled_since_us_ = 0;
        }
        if (!at_target) {
            const int turn_sign = motion_ == Motion::kLeft ? 1 : -1;
            // Scale the turn P gain by the requested speed: use the RPM ceiling
            // far from the goal, then slow down inside the configured angle.
            const double turn_kp = base_rpm / HEADING_TURN_SLOWDOWN_DEGREES;
            const auto turn =
                HeadingPd::Update(0, heading_target_degrees_, imu.heading_degrees, imu.gyro_z_dps,
                                  turn_kp, HEADING_PD_KD_RPM_PER_DPS, base_rpm);
            // Do not reverse wheel directions in a running command.
            const double forward_correction = turn_sign * turn.correction_rpm;
            if (forward_correction > 0) {
                targets.correction_rpm =
                    turn_sign * std::clamp(forward_correction,
                                           std::min(HEADING_TURN_MIN_RPM, base_rpm), base_rpm);
            }
            targets.left_rpm = -targets.correction_rpm;
            targets.right_rpm = targets.correction_rpm;
        }
    } else {
        const double signed_rpm = motion_ == Motion::kBackward ? -base_rpm : base_rpm;
        const double correction_limit = std::min(HEADING_PD_MAX_CORRECTION_RPM, base_rpm * 0.5);
        targets = HeadingPd::Update(signed_rpm, heading_target_degrees_, imu.heading_degrees,
                                    imu.gyro_z_dps, HEADING_PD_KP_RPM_PER_DEG,
                                    HEADING_PD_KD_RPM_PER_DPS, correction_limit);
    }
    const int left_percent = PercentForRpm(targets.left_rpm, SPEED_PI_LEFT_REFERENCE_RPM);
    const int right_percent = PercentForRpm(targets.right_rpm, SPEED_PI_RIGHT_REFERENCE_RPM);
    if (!SetRequestedWheelsLocked(left_percent, right_percent))
        return false;
    encoder_.SetDrive((left_percent > 0) - (left_percent < 0),
                      (right_percent > 0) - (right_percent < 0), now_us);
    heading_error_degrees_ = error;
    heading_correction_rpm_ = targets.correction_rpm;
    last_heading_pd_us_ = now_us;
    heading_fault_ = "none";
    return true;
}

bool L298nController::UpdateSpeedPiLocked(int64_t now_us) {
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
    const double left_target_rpm = TargetRpm(requested_left_percent_, SPEED_PI_LEFT_REFERENCE_RPM);
    const double right_target_rpm =
        TargetRpm(requested_right_percent_, SPEED_PI_RIGHT_REFERENCE_RPM);
    const double left_feedforward = FeedforwardPercent(
        requested_left_percent_, heading_turn_active_ ? HEADING_TURN_LEFT_FRICTION_PWM_PERCENT : 0);
    const double right_feedforward =
        FeedforwardPercent(requested_right_percent_,
                           heading_turn_active_ ? HEADING_TURN_RIGHT_FRICTION_PWM_PERCENT : 0);
    const int left =
        requested_left_percent_ == 0
            ? 0
            : left_speed_pi_.Update(left_target_rpm, encoder.left.rpm, left_feedforward,
                                    bounded_dt_s, SPEED_PI_LEFT_KP, SPEED_PI_LEFT_KI);
    const int right =
        requested_right_percent_ == 0
            ? 0
            : right_speed_pi_.Update(right_target_rpm, encoder.right.rpm, right_feedforward,
                                     bounded_dt_s, SPEED_PI_RIGHT_KP, SPEED_PI_RIGHT_KI);
    last_speed_pi_us_ = now_us;
    if (!ApplyMotorPwmLocked(left, right))
        return false;
    command_interface_.RecordSpeedSample(left_target_rpm, right_target_rpm, encoder.left.rpm,
                                         encoder.right.rpm, left, right);
    return true;
}

}  // namespace ruby_robot
