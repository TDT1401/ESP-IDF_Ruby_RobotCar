#include "controller_internal.h"

#include <esp_random.h>

namespace ruby_robot {

namespace {

bool ValidWebDriveRequest(const L298nWebDriveRequest& request) {
    return request.speed_percent >= 0 && request.speed_percent <= 100 &&
           request.joystick_x >= -1000 && request.joystick_x <= 1000 &&
           request.joystick_y >= -1000 && request.joystick_y <= 1000 &&
           (request.joystick || (request.joystick_x == 0 && request.joystick_y == 0));
}

}  // namespace

void L298nController::SetWebConnected(bool connected) {
    std::lock_guard lock(mutex_);
    web_control_.SetConnected(connected);
    if (!connected && web_control_.active()) {
        EndWebControlLocked();
    } else if (!connected && command_interface_.Active()) {
        StopLocked();
    }
}

esp_err_t L298nController::BeginWebControl(uint32_t* session) {
    if (session == nullptr)
        return ESP_ERR_INVALID_ARG;
    std::lock_guard lock(mutex_);
    const int64_t now = esp_timer_get_time();
    if (!web_control_.connected() || !MotionAllowed() || command_interface_.Active())
        return ESP_ERR_INVALID_STATE;
    if (!web_control_.CanBegin(now))
        return ESP_ERR_INVALID_STATE;
    if (web_control_.active())
        EndWebControlLocked();
    if (fault_ != ESP_OK)
        return fault_;
    StopLocked();
    speed_pi_fault_ = "none";
    if (fault_ != ESP_OK)
        return fault_;
    if (!command_interface_.BeginManual())
        return ESP_ERR_INVALID_STATE;
#ifdef CONFIG_PM_ENABLE
    if (!Check(esp_pm_lock_acquire(web_sleep_lock_))) {
        command_interface_.ReleaseManual();
        return ESP_FAIL;
    }
    web_sleep_lock_held_ = true;
#endif
    proximity_warning_enabled_ = true;
    // Zero is reserved for an expired/released session.
    uint32_t token = esp_random();
    if (token == 0)
        token = 1;
    web_control_.Begin(token, now);
    *session = token;
    return ESP_OK;
}

esp_err_t L298nController::ReleaseWebControl(const L298nWebDriveRequest& request) {
    if (!ValidWebDriveRequest(request))
        return ESP_ERR_INVALID_ARG;
    std::lock_guard lock(mutex_);
    const int64_t now = esp_timer_get_time();
    if (!web_control_.Accept(request.session, request.sequence, now))
        return ESP_ERR_INVALID_STATE;
    EndWebControlLocked();
    return ESP_OK;
}

esp_err_t L298nController::WebCommand(const L298nWebDriveRequest& request, Motion motion) {
    if (!ValidWebDriveRequest(request) ||
        (request.joystick && JoystickMotion(request.joystick_x, request.joystick_y) != motion))
        return ESP_ERR_INVALID_ARG;
    std::lock_guard lock(mutex_);
    const int64_t now = esp_timer_get_time();
    if (!web_control_.Accept(request.session, request.sequence, now))
        return ESP_ERR_INVALID_STATE;
    proximity_warning_enabled_ = request.proximity_warning_enabled;
    if (!MotionAllowed() || fault_ != ESP_OK) {
        EndWebControlLocked();
        return ESP_ERR_INVALID_STATE;
    }
    web_control_.Refresh(now);
    web_speed_percent_ = request.speed_percent;
    if (motion == Motion::kStop || request.speed_percent == 0) {
        StopLocked();
    } else if (request.joystick) {
        const auto wheels =
            MixJoystick(request.joystick_x, request.joystick_y, request.speed_percent);
        if (wheels.left == 0 && wheels.right == 0) {
            StopLocked();
        } else {
            if (!web_joystick_active_ && moving_) {
                StopOutputsLocked();
                web_joystick_pending_ = true;
                start_at_us_ = now + kDirectionDelayUs;
            }
            web_joystick_active_ = true;
            if (!ApplyWebDriveLocked(wheels.left, wheels.right)) {
                StopLocked();
                EndWebControlLocked();
                return ESP_FAIL;
            }
            motion_ = motion;
            moving_ = !web_joystick_pending_;
        }
    } else {
        if (web_joystick_active_) {
            StopLocked();
        }
        // Heartbeats refresh the deadline without interrupting PWM or the direction delay.
        if (motion_ != motion) {
            StopLocked();
            if (fault_ != ESP_OK || !PrepareMotionLocked(motion)) {
                EndWebControlLocked();
                return ESP_FAIL;
            }
        }
        if (moving_ && !ApplySpeedLocked()) {
            EndWebControlLocked();
            return ESP_FAIL;
        }
    }
    return fault_;
}

void L298nController::UpdateEmotion(const char* emotion, bool speaking) {
    std::lock_guard lock(mutex_);
    if (speaking && IsSleepEmotion(emotion)) {
        if (!web_control_.active())
            StopLocked();
    }
}

void L298nController::ReleaseWebSleepLockLocked() {
#ifdef CONFIG_PM_ENABLE
    if (web_sleep_lock_held_) {
        Check(esp_pm_lock_release(web_sleep_lock_));
        web_sleep_lock_held_ = false;
    }
#endif
}

void L298nController::EndWebControlLocked() {
    web_control_.Revoke();
    ReleaseWebSleepLockLocked();
    StopLocked();
    command_interface_.ReleaseManual();
}

}  // namespace ruby_robot
