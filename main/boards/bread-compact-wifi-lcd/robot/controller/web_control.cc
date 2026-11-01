#include "controller_internal.h"

#include <esp_random.h>

namespace ruby_robot {

void L298nController::SetWebConnected(bool connected) {
    std::lock_guard lock(mutex_);
    web_control_.SetConnected(connected);
    if (!connected && web_control_.active()) {
        EndWebControlLocked();
    }
}

esp_err_t L298nController::BeginWebControl(uint32_t* session) {
    if (session == nullptr)
        return ESP_ERR_INVALID_ARG;
    std::lock_guard lock(mutex_);
    const int64_t now = esp_timer_get_time();
    if (!web_control_.connected() || !MotionAllowed())
        return ESP_ERR_INVALID_STATE;
    if (!web_control_.CanBegin(now))
        return ESP_ERR_INVALID_STATE;
    if (web_control_.active())
        EndWebControlLocked();
    if (fault_ != ESP_OK)
        return fault_;
    StopLocked();
    if (fault_ != ESP_OK)
        return fault_;
#ifdef CONFIG_PM_ENABLE
    if (!Check(esp_pm_lock_acquire(web_sleep_lock_)))
        return ESP_FAIL;
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

esp_err_t L298nController::WebCommand(uint32_t session, uint32_t sequence, L298nWebCommand command,
                                      int speed, bool proximity_warning_enabled, int joystick_x,
                                      int joystick_y) {
    if (speed < 0 || speed > 100 || joystick_x < -1000 || joystick_x > 1000 || joystick_y < -1000 ||
        joystick_y > 1000)
        return ESP_ERR_INVALID_ARG;
    Motion motion;
    switch (command) {
        case L298nWebCommand::kForward:
            motion = Motion::kForward;
            break;
        case L298nWebCommand::kBackward:
            motion = Motion::kBackward;
            break;
        case L298nWebCommand::kLeft:
            motion = Motion::kLeft;
            break;
        case L298nWebCommand::kRight:
            motion = Motion::kRight;
            break;
        case L298nWebCommand::kJoystick:
            motion = joystick_y > 0   ? Motion::kForward
                     : joystick_y < 0 ? Motion::kBackward
                     : joystick_x < 0 ? Motion::kLeft
                     : joystick_x > 0 ? Motion::kRight
                                      : Motion::kStop;
            break;
        case L298nWebCommand::kStop:
        case L298nWebCommand::kRelease:
            motion = Motion::kStop;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }
    std::lock_guard lock(mutex_);
    const int64_t now = esp_timer_get_time();
    if (!web_control_.Accept(session, sequence, now))
        return ESP_ERR_INVALID_STATE;
    proximity_warning_enabled_ = proximity_warning_enabled;
    if (command == L298nWebCommand::kRelease) {
        EndWebControlLocked();
        return ESP_OK;
    }
    if (!MotionAllowed() || fault_ != ESP_OK) {
        EndWebControlLocked();
        return ESP_ERR_INVALID_STATE;
    }
    web_control_.Refresh(now);
    web_speed_percent_ = speed;
    if (motion == Motion::kStop || speed == 0) {
        StopLocked();
    } else if (command == L298nWebCommand::kJoystick) {
        const auto wheels = MixJoystick(joystick_x, joystick_y, speed);
        if (wheels.left == 0 && wheels.right == 0) {
            StopLocked();
        } else {
            if (!web_joystick_active_ && moving_)
                StopOutputsLocked();
            web_joystick_active_ = true;
            if (!ApplyWebDriveLocked(wheels.left, wheels.right)) {
                StopLocked();
                EndWebControlLocked();
                return ESP_FAIL;
            }
            motion_ = motion;
            moving_ = true;
            deadline_us_ = web_control_.deadline_us();
        }
    } else {
        if (web_joystick_active_) {
            StopLocked();
            web_joystick_active_ = false;
        }
        // Heartbeats refresh the deadline without interrupting PWM or the direction delay.
        if (motion_ != motion) {
            StopLocked();
            if (fault_ != ESP_OK || !PrepareMotionLocked(motion)) {
                EndWebControlLocked();
                return ESP_FAIL;
            }
        }
        deadline_us_ = web_control_.deadline_us();
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
}

}  // namespace ruby_robot
