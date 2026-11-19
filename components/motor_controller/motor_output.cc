#include "controller_internal.h"

#include "imu.h"

#include <array>

namespace ruby_robot {
namespace {

// Order matches Wheel: front left/right, rear left/right.
std::array<int, 4> WheelOutputs(int left, int right) {
    return {left, right, left, right};
}

}  // namespace

void MotorController::StopOutputsLocked() {
    const int64_t now_us = esp_timer_get_time();
    last_motor_stop_us_ = now_us;
    SetMpu6050MotorActive(false);
    heading_hold_active_ = false;
    heading_turn_active_ = false;
    heading_turn_complete_ = false;
    turn_settled_since_us_ = 0;
    heading_target_degrees_ = 0;
    heading_error_degrees_ = 0;
    heading_correction_rpm_ = 0;
    last_heading_pd_us_ = 0;
    applied_left_pwm_percent_ = 0;
    applied_right_pwm_percent_ = 0;
    requested_left_percent_ = 0;
    requested_right_percent_ = 0;
    last_speed_pi_us_ = 0;
    left_speed_pi_.Reset();
    right_speed_pi_.Reset();
    encoder_.SetDrive(0, 0, now_us);
    Check(driver_.Stop());
    motion_ = Motion::kStop;
    moving_ = false;
    web_left_output_percent_ = 0;
    web_right_output_percent_ = 0;
    web_joystick_pending_ = false;
    start_at_us_ = 0;
#ifdef CONFIG_PM_ENABLE
    if (sleep_lock_held_) {
        Check(esp_pm_lock_release(sleep_lock_));
        sleep_lock_held_ = false;
    }
#endif
}

bool MotorController::ApplyMotorPwmLocked(int left_percent, int right_percent) {
    const auto outputs = WheelOutputs(left_percent, right_percent);
    for (unsigned i = 0; i < outputs.size(); ++i) {
        if (!Check(driver_.SetPwm(static_cast<Wheel>(i), outputs[i])))
            return false;
    }
    applied_left_pwm_percent_ = left_percent;
    applied_right_pwm_percent_ = right_percent;
    return true;
}

bool MotorController::ApplyWheelDirectionsLocked(int left, int right) {
    const auto outputs = WheelOutputs(left, right);
    for (unsigned i = 0; i < outputs.size(); ++i) {
        const auto wheel = static_cast<Wheel>(i);
        // Disable a stopped wheel before clearing its direction pins.
        if (outputs[i] == 0 && !Check(driver_.SetPwm(wheel, 0)))
            return false;
        if (!Check(driver_.SetDirection(wheel, outputs[i])))
            return false;
    }
    return true;
}

bool MotorController::ApplyWebDriveLocked(int left, int right) {
    encoder_.Update(esp_timer_get_time());
    if (web_joystick_pending_) {
        web_left_output_percent_ = left;
        web_right_output_percent_ = right;
        return true;
    }
    const bool direction_changed = (left != 0 && web_left_output_percent_ != 0 &&
                                    (left < 0) != (web_left_output_percent_ < 0)) ||
                                   (right != 0 && web_right_output_percent_ != 0 &&
                                    (right < 0) != (web_right_output_percent_ < 0));
    if (direction_changed) {
        StopOutputsLocked();
        web_joystick_pending_ = true;
        web_left_output_percent_ = left;
        web_right_output_percent_ = right;
        start_at_us_ = esp_timer_get_time() + kDirectionDelayUs;
        return true;
    }

    // Each front encoder provides speed feedback for its side.
    const bool ok = ApplyWheelDirectionsLocked(left, right) &&
                    command_interface_.SubmitManual({left, right}) &&
                    ApplyCommandLocked();
    if (ok) {
        web_left_output_percent_ = left;
        web_right_output_percent_ = right;
        encoder_.SetDrive(left, right, esp_timer_get_time());
        SetMpu6050MotorActive(left != 0 || right != 0);
    }
    return ok;
}

bool MotorController::ApplyDirectionLocked() {
    const bool left_forward = motion_ == Motion::kForward || motion_ == Motion::kRight;
    const bool right_forward = motion_ == Motion::kForward || motion_ == Motion::kLeft;
    // Keep logical wheel signs before applying physical motor wiring inversion.
    const int encoder_left = left_forward ? 1 : -1;
    const int encoder_right = right_forward ? 1 : -1;
    encoder_.Update(esp_timer_get_time());
    const bool ok = ApplyWheelDirectionsLocked(encoder_left, encoder_right);
    if (ok)
        encoder_.SetDrive(encoder_left, encoder_right, esp_timer_get_time());
    return ok;
}

}  // namespace ruby_robot
