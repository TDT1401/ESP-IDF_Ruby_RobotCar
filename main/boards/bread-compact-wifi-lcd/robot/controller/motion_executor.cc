#include "controller_internal.h"

#include "../sensors/mpu6050_imu.h"
#include "application.h"

namespace ruby_robot {

void L298nController::StopLocked() {
    web_joystick_active_ = false;
    command_interface_.StopMotion(esp_timer_get_time());
    StopOutputsLocked();
}

void L298nController::StopFailedMotionLocked(int64_t now_us) {
    if (web_control_.active()) {
        EndWebControlLocked();
        return;
    }
    const bool active_command = command_interface_.Active();
    StopLocked();
    if (active_command)
        command_interface_.Fail(now_us);
}

int L298nController::ActiveSpeedLocked() const {
    return command_interface_.Active() ? command_interface_.speed_percent() : web_speed_percent_;
}

bool L298nController::PrepareMotionLocked(Motion motion) {
#ifdef CONFIG_PM_ENABLE
    if (!Check(esp_pm_lock_acquire(sleep_lock_)))
        return false;
    sleep_lock_held_ = true;
#endif
    motion_ = motion;
    start_at_us_ = esp_timer_get_time() + kDirectionDelayUs;
    deadline_us_ = 0;
    return true;
}

bool L298nController::MotionAllowed() {
    const auto state = Application::GetInstance().GetDeviceState();
    return state != kDeviceStateUpgrading && state != kDeviceStateFatalError &&
           state != kDeviceStateWifiConfiguring;
}

void L298nController::Tick() {
    std::lock_guard lock(mutex_);
    const int64_t now = esp_timer_get_time();
    encoder_.Update(now);
    if (web_control_.active() && (web_control_.Expired(now) || !MotionAllowed())) {
        EndWebControlLocked();
        return;
    }
    if (!MotionAllowed()) {
        if (motion_ != Motion::kStop)
            StopLocked();
        return;
    }
    if (command_interface_.Expired(now)) {
        const bool angle_turn = command_interface_.IsAngleTurn();
        const bool completion_ready = !angle_turn && command_interface_.CompletionReady();
        if (angle_turn)
            heading_fault_ = "turn_timeout";
        StopLocked();
        if (fault_ == ESP_OK && completion_ready)
            command_interface_.Complete(now);
        else
            command_interface_.Fail(now);
        return;
    }
    if (motion_ == Motion::kStop)
        return;
    if (deadline_us_ > 0 && now >= deadline_us_) {
        StopLocked();
        return;
    }
    if (moving_) {
        if (!UpdateHeadingPdLocked(now)) {
            StopFailedMotionLocked(now);
            return;
        }
        if (heading_turn_complete_) {
            StopLocked();
            command_interface_.Complete(now);
            return;
        }
        if (!UpdateSpeedPiLocked(now)) {
            StopFailedMotionLocked(now);
            return;
        }
    }
    if (web_joystick_active_) {
        if (web_joystick_pending_ && now >= start_at_us_) {
            const int left = web_left_output_percent_;
            const int right = web_right_output_percent_;
            web_joystick_pending_ = false;
            if (!ApplyWebDriveLocked(left, right)) {
                StopFailedMotionLocked(now);
                return;
            }
            moving_ = true;
        }
        return;
    }
    if (!moving_ && now >= start_at_us_) {
        if (!ApplyDirectionLocked()) {
            StopFailedMotionLocked(now);
            return;
        }
        moving_ = true;
        SetMpu6050MotorActive(true);
        if (!ApplySpeedLocked()) {
            StopFailedMotionLocked(now);
        }
    }
}

bool L298nController::ApplySpeedLocked() {
    if (heading_hold_active_)
        return UpdateHeadingPdLocked(esp_timer_get_time());
    const int speed = ActiveSpeedLocked();
    if (command_interface_.mode() == CommandMode::kManual &&
        !command_interface_.SubmitManual({speed, speed}))
        return false;
    return ApplyCommandLocked();
}

bool L298nController::ApplyCommandLocked() {
    const auto wheels = command_interface_.wheels();
    return SetRequestedWheelsLocked(wheels.left_percent, wheels.right_percent);
}

}  // namespace ruby_robot
