#include "controller_internal.h"

#include <cstdlib>

#include "../sensors/mpu6050_imu.h"

namespace ruby_robot {

void L298nController::StopOutputsLocked() {
    SetMpu6050MotorActive(false);
    applied_left_pwm_percent_ = 0;
    applied_right_pwm_percent_ = 0;
    encoder_.SetDrive(0, 0, esp_timer_get_time());
    Check(driver_.Stop());
    motion_ = Motion::kStop;
    moving_ = false;
    web_left_output_percent_ = 0;
    web_right_output_percent_ = 0;
    start_at_us_ = 0;
    deadline_us_ = 0;
#ifdef CONFIG_PM_ENABLE
    if (sleep_lock_held_) {
        Check(esp_pm_lock_release(sleep_lock_));
        sleep_lock_held_ = false;
    }
#endif
}

bool L298nController::SetWheelPwmLocked(Wheel wheel, int percent) {
    return Check(driver_.SetPwm(wheel, percent));
}

bool L298nController::SetWheelDirectionLocked(Wheel wheel, int sign, bool inverted) {
    return Check(driver_.SetDirection(wheel, sign, inverted));
}

bool L298nController::ApplyMotorPwmLocked(int left_percent, int right_percent) {
    const bool ok = SetWheelPwmLocked(Wheel::kLeft, left_percent) &&
                    SetWheelPwmLocked(Wheel::kRight, right_percent);
    if (ok) {
        applied_left_pwm_percent_ = left_percent;
        applied_right_pwm_percent_ = right_percent;
    }
    return ok;
}

int L298nController::CurrentPwmPercentLocked(int side) const {
    if (!moving_)
        return 0;
    if (web_control_.active() && web_joystick_active_)
        return std::abs(side == 0 ? web_left_output_percent_ : web_right_output_percent_);
    return side == 0 ? applied_left_pwm_percent_ : applied_right_pwm_percent_;
}

int L298nController::CurrentLeftPwmPercentLocked() const { return CurrentPwmPercentLocked(0); }

int L298nController::CurrentRightPwmPercentLocked() const { return CurrentPwmPercentLocked(1); }

bool L298nController::ApplyWebDriveLocked(int left, int right) {
    encoder_.Update(esp_timer_get_time());
    const bool direction_changed = (left != 0 && web_left_output_percent_ != 0 &&
                                    (left < 0) != (web_left_output_percent_ < 0)) ||
                                   (right != 0 && web_right_output_percent_ != 0 &&
                                    (right < 0) != (web_right_output_percent_ < 0));
    if (direction_changed)
        StopOutputsLocked();

    bool ok = true;
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_LEFT
    constexpr bool invert_left = true;
#else
    constexpr bool invert_left = false;
#endif
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_RIGHT
    constexpr bool invert_right = true;
#else
    constexpr bool invert_right = false;
#endif
    if (left == 0)
        ok = SetWheelPwmLocked(Wheel::kLeft, 0) && ok;
    if (right == 0)
        ok = SetWheelPwmLocked(Wheel::kRight, 0) && ok;
    ok = SetWheelDirectionLocked(Wheel::kLeft, left, invert_left) && ok;
    ok = SetWheelDirectionLocked(Wheel::kRight, right, invert_right) && ok;
    if (left != 0)
        ok = SetWheelPwmLocked(Wheel::kLeft, left) && ok;
    if (right != 0)
        ok = SetWheelPwmLocked(Wheel::kRight, right) && ok;
    if (ok) {
        web_left_output_percent_ = left;
        web_right_output_percent_ = right;
        encoder_.SetDrive(left, right, esp_timer_get_time());
        SetMpu6050MotorActive(left != 0 || right != 0);
    }
    return ok;
}

bool L298nController::ApplyDirectionLocked() {
    const bool left_forward = motion_ == Motion::kForward || motion_ == Motion::kRight;
    const bool right_forward = motion_ == Motion::kForward || motion_ == Motion::kLeft;
    // Keep logical wheel signs before applying physical motor wiring inversion.
    const int encoder_left = left_forward ? 1 : -1;
    const int encoder_right = right_forward ? 1 : -1;
    encoder_.Update(esp_timer_get_time());
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_LEFT
    constexpr bool invert_left = true;
#else
    constexpr bool invert_left = false;
#endif
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_RIGHT
    constexpr bool invert_right = true;
#else
    constexpr bool invert_right = false;
#endif
    const bool ok = SetWheelDirectionLocked(Wheel::kLeft, encoder_left, invert_left) &&
                    SetWheelDirectionLocked(Wheel::kRight, encoder_right, invert_right);
    if (ok)
        encoder_.SetDrive(encoder_left, encoder_right, esp_timer_get_time());
    return ok;
}

}  // namespace ruby_robot
