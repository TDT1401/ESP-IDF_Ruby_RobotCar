#include "controller_internal.h"

namespace ruby_robot {

void L298nController::StopOutputsLocked() {
    SetMpu6050MotorActive(false);
    applied_left_pwm_percent_ = 0;
    applied_right_pwm_percent_ = 0;
    encoder_.SetDrive(0, 0, esp_timer_get_time());
    // Disable both bridges before touching any direction pin. Attempt every
    // output even if an earlier operation fails. Stop means coast, not brake.
    Check(ledc_stop(kMode, kLeftChannel, 0));
    Check(ledc_stop(kMode, kRightChannel, 0));
    for (auto pin :
         {MOTOR_LEFT_IN1_PIN, MOTOR_LEFT_IN2_PIN, MOTOR_RIGHT_IN1_PIN, MOTOR_RIGHT_IN2_PIN}) {
        Check(gpio_set_level(pin, 0));
    }
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

bool L298nController::SetWheelPwmLocked(ledc_channel_t channel, int percent) {
    if (percent == 0)
        return Check(ledc_stop(kMode, channel, 0));
    const uint32_t duty = static_cast<uint32_t>(std::abs(percent)) * 1023 / 100;
    return Check(ledc_set_duty(kMode, channel, duty)) && Check(ledc_update_duty(kMode, channel));
}

bool L298nController::SetWheelDirectionLocked(gpio_num_t in1, gpio_num_t in2, int sign,
                                              bool inverted) {
    if (sign == 0)
        return Check(gpio_set_level(in1, 0)) && Check(gpio_set_level(in2, 0));
    const bool forward = (sign > 0) != inverted;
    return Check(gpio_set_level(in1, forward)) && Check(gpio_set_level(in2, !forward));
}

bool L298nController::ApplyMotorPwmLocked(int left_percent, int right_percent) {
    const bool ok = SetWheelPwmLocked(kLeftChannel, left_percent) &&
                    SetWheelPwmLocked(kRightChannel, right_percent);
    if (ok) {
        applied_left_pwm_percent_ = left_percent;
        applied_right_pwm_percent_ = right_percent;
    }
    return ok;
}

int L298nController::CurrentPwmPercentLocked(int side) const {
    if (!moving_)
        return 0;
    if (web_manual_ && web_joystick_active_)
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
        ok = SetWheelPwmLocked(kLeftChannel, 0) && ok;
    if (right == 0)
        ok = SetWheelPwmLocked(kRightChannel, 0) && ok;
    ok = SetWheelDirectionLocked(MOTOR_LEFT_IN1_PIN, MOTOR_LEFT_IN2_PIN, left, invert_left) && ok;
    ok = SetWheelDirectionLocked(MOTOR_RIGHT_IN1_PIN, MOTOR_RIGHT_IN2_PIN, right, invert_right) &&
         ok;
    if (left != 0)
        ok = SetWheelPwmLocked(kLeftChannel, left) && ok;
    if (right != 0)
        ok = SetWheelPwmLocked(kRightChannel, right) && ok;
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
    const bool ok = SetWheelDirectionLocked(MOTOR_LEFT_IN1_PIN, MOTOR_LEFT_IN2_PIN, encoder_left,
                                            invert_left) &&
                    SetWheelDirectionLocked(MOTOR_RIGHT_IN1_PIN, MOTOR_RIGHT_IN2_PIN, encoder_right,
                                            invert_right);
    if (ok)
        encoder_.SetDrive(encoder_left, encoder_right, esp_timer_get_time());
    return ok;
}

}  // namespace ruby_robot
