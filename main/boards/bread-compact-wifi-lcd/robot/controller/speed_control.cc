#include "controller_internal.h"

#include <algorithm>
#include <cstdlib>

#include "../../config.h"

namespace ruby_robot {
namespace {

static_assert(SPEED_PI_REFERENCE_PWM_PERCENT > 0 && SPEED_PI_REFERENCE_PWM_PERCENT <= 100);
static_assert(SPEED_PI_LEFT_REFERENCE_RPM >= 0 && SPEED_PI_RIGHT_REFERENCE_RPM >= 0);
static_assert(SPEED_PI_LEFT_KP >= 0 && SPEED_PI_LEFT_KI >= 0);
static_assert(SPEED_PI_RIGHT_KP >= 0 && SPEED_PI_RIGHT_KI >= 0);

double TargetRpm(int requested_percent, double reference_rpm) {
    return std::abs(requested_percent) * reference_rpm / SPEED_PI_REFERENCE_PWM_PERCENT;
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
    if ((left_percent > 0) != (requested_left_percent_ > 0) ||
        (left_percent < 0) != (requested_left_percent_ < 0))
        left_speed_pi_.Reset();
    if ((right_percent > 0) != (requested_right_percent_ > 0) ||
        (right_percent < 0) != (requested_right_percent_ < 0))
        right_speed_pi_.Reset();
    int left_output = std::abs(left_percent);
    int right_output = std::abs(right_percent);
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
    const double dt_s = last_speed_pi_us_ == 0
                            ? kSpeedPiPeriodUs / 1000000.0
                            : (now_us - last_speed_pi_us_) / 1000000.0;
    if (dt_s > 0.5) {
        left_speed_pi_.Reset();
        right_speed_pi_.Reset();
    }
    const double bounded_dt_s = std::min(dt_s, 0.25);
    const int left = requested_left_percent_ == 0
                         ? 0
                         : left_speed_pi_.Update(TargetRpm(requested_left_percent_,
                                                           SPEED_PI_LEFT_REFERENCE_RPM),
                                                 encoder.left.rpm, std::abs(requested_left_percent_),
                                                 bounded_dt_s, SPEED_PI_LEFT_KP, SPEED_PI_LEFT_KI);
    const int right = requested_right_percent_ == 0
                          ? 0
                          : right_speed_pi_.Update(TargetRpm(requested_right_percent_,
                                                              SPEED_PI_RIGHT_REFERENCE_RPM),
                                                   encoder.right.rpm, std::abs(requested_right_percent_),
                                                   bounded_dt_s, SPEED_PI_RIGHT_KP, SPEED_PI_RIGHT_KI);
    last_speed_pi_us_ = now_us;
    return ApplyMotorPwmLocked(left, right);
}

}  // namespace ruby_robot
