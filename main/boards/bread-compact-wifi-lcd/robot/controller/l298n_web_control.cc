#include "l298n_internal.h"

namespace ruby_robot {

void L298nController::SetWebConnected(bool connected) {
    std::lock_guard lock(mutex_);
    web_connected_ = connected;
    if (!connected)
        if (!connected && route_active_ && strcmp(control_owner_, "web") == 0)
            CancelRouteLocked();
    if (!connected && strcmp(control_owner_, "web") == 0 &&
        (distance_move_state_ == DistanceMoveState::kPending ||
         distance_move_state_ == DistanceMoveState::kMoving))
        StopLocked();
    if (!connected && web_manual_) {
        EndWebControlLocked();
    }
}

esp_err_t L298nController::BeginWebControl(uint32_t* session) {
    if (session == nullptr)
        return ESP_ERR_INVALID_ARG;
    std::lock_guard lock(mutex_);
    const int64_t now = esp_timer_get_time();
    if (!web_connected_ || !MotionAllowed())
        return ESP_ERR_INVALID_STATE;
    if (web_session_ != 0 && now < web_deadline_us_)
        return ESP_ERR_INVALID_STATE;
    if (web_manual_)
        EndWebControlLocked();
    if (fault_ != ESP_OK)
        return fault_;
    CancelRouteLocked();
    DisableAutonomyLocked();
    StopLocked();
    if (fault_ != ESP_OK)
        return fault_;
#ifdef CONFIG_PM_ENABLE
    if (!Check(esp_pm_lock_acquire(web_sleep_lock_)))
        return ESP_FAIL;
    web_sleep_lock_held_ = true;
#endif
    web_manual_ = true;
    proximity_warning_enabled_ = true;
    web_sequence_ = 0;
    // Zero is reserved for an expired/released session.
    web_session_ = esp_random();
    if (web_session_ == 0)
        web_session_ = 1;
    web_deadline_us_ = now + kWebTimeoutMs * 1000LL;
    *session = web_session_;
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
    if (!web_connected_ || session == 0 || session != web_session_ || sequence <= web_sequence_ ||
        now >= web_deadline_us_)
        return ESP_ERR_INVALID_STATE;
    web_sequence_ = sequence;
    proximity_warning_enabled_ = proximity_warning_enabled;
    if (command == L298nWebCommand::kRelease) {
        EndWebControlLocked();
        return ESP_OK;
    }
    if (!MotionAllowed() || fault_ != ESP_OK) {
        EndWebControlLocked();
        return ESP_ERR_INVALID_STATE;
    }
    web_deadline_us_ = now + kWebTimeoutMs * 1000LL;
    web_speed_percent_ = speed;
    if (motion == Motion::kStop || speed == 0) {
        StopLocked();
    } else if (command == L298nWebCommand::kJoystick) {
        int left = joystick_y + joystick_x;
        int right = joystick_y - joystick_x;
        const int peak = std::max(std::abs(left), std::abs(right));
        if (peak > 1000) {
            left = left * 1000 / peak;
            right = right * 1000 / peak;
        }
        // A full sideways stick gets the same turn power as a direction button;
        // partial deflection still scales the output smoothly.
        const int drive_speed =
            joystick_y == 0 && joystick_x != 0 ? std::max(speed, kTurnSpeedPercent) : speed;
        left = left * drive_speed / 1000;
        right = right * drive_speed / 1000;
        web_speed_percent_ = drive_speed;
        if (left == 0 && right == 0) {
            StopLocked();
        } else {
            if (!web_joystick_active_ && moving_)
                StopOutputsLocked();
            web_joystick_active_ = true;
            if (!ApplyWebDriveLocked(left, right)) {
                StopLocked();
                EndWebControlLocked();
                return ESP_FAIL;
            }
            motion_ = motion;
            moving_ = true;
            deadline_us_ = web_deadline_us_;
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
        deadline_us_ = web_deadline_us_;
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
        response_active_ = true;
        manual_override_ = true;
        DisableAutonomyLocked();
        if (!web_manual_)
            StopLocked();
        return;
    }
    if (!speaking && response_active_)
        manual_override_ = false;
    response_active_ = speaking;
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
    web_session_ = 0;
    web_sequence_ = 0;
    web_deadline_us_ = 0;
    web_manual_ = false;
    ReleaseWebSleepLockLocked();
    manual_override_ = response_active_;
    StopLocked();
}

}  // namespace ruby_robot
