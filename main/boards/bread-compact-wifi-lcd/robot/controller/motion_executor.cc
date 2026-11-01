#include "controller_internal.h"

#include "../sensors/mpu6050_imu.h"
#include "application.h"

namespace ruby_robot {

void L298nController::StopLocked() {
    web_joystick_active_ = false;
    StopOutputsLocked();
}

int L298nController::ActiveSpeedLocked() const {
    return web_speed_percent_;
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
    encoder_.Update(now);  // Telemetry only; motor PWM does not depend on encoder readings.
    if (web_control_.active() && (web_control_.Expired(now) || !MotionAllowed())) {
        EndWebControlLocked();
        return;
    }
    if (!MotionAllowed()) {
        if (motion_ != Motion::kStop)
            StopLocked();
        return;
    }
    if (motion_ == Motion::kStop || web_joystick_active_)
        return;
    if (deadline_us_ > 0 && now >= deadline_us_) {
        StopLocked();
        return;
    }
    if (!moving_ && now >= start_at_us_) {
        if (!ApplyDirectionLocked()) {
            StopLocked();
            return;
        }
        moving_ = true;
        SetMpu6050MotorActive(true);
        if (!ApplySpeedLocked())
            StopLocked();
    }
}

bool L298nController::ApplySpeedLocked() {
    const int speed = ActiveSpeedLocked();
    return ApplyMotorPwmLocked(speed, speed);
}

}  // namespace ruby_robot
