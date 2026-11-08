#include "controller_internal.h"

#include "../sensors/mpu6050_imu.h"
#include "application.h"
#include "../../config.h"

#include <cmath>
#include <numbers>

namespace ruby_robot {

void L298nController::StopLocked() {
    web_joystick_active_ = false;
    command_interface_.StopMotion(esp_timer_get_time());
    StopOutputsLocked();
}

void L298nController::FinishCommandLocked(int64_t now_us, bool completed, const char* fault) {
    web_joystick_active_ = false;
    StopOutputsLocked();
    // Keep the mode until its result is recorded; StopMotion would turn it idle.
    if (completed && fault_ == ESP_OK)
        command_interface_.Complete(now_us);
    else
        command_interface_.Fail(now_us, fault_ == ESP_OK ? fault : "motor_error");
}

void L298nController::StopFailedMotionLocked(int64_t now_us, const char* fault) {
    if (web_control_.active()) {
        EndWebControlLocked();
        return;
    }
    if (command_interface_.Active())
        FinishCommandLocked(now_us, false, fault);
    else
        StopLocked();
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
    UpdateOdometryLocked(now);
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
        if (angle_turn)
            heading_fault_ = "turn_timeout";
        FinishCommandLocked(now, false, angle_turn ? "turn_timeout" : "timeout");
        return;
    }
    if (motion_ == Motion::kStop)
        return;
    const auto encoder = encoder_.Snapshot();
    const auto goal_pose = command_interface_.IsNavigation() ? NavigationPoseLocked()
                                                              : odometry_.Snapshot();
    if (!command_interface_.UpdateMotion(goal_pose, encoder.left.rpm, encoder.right.rpm, now)) {
        StopFailedMotionLocked(now, command_interface_.fault());
        return;
    }
    if (command_interface_.CompletionReady()) {
        FinishCommandLocked(now, true);
        return;
    }
    if (command_interface_.IsNavigation() && sensor_valid_ &&
        now - last_sensor_update_us_ <= kSensorStaleMs * 1000LL &&
        measured_distance_cm_ <=
            ScaleDistanceForSpeed(kDefaultObstacleDistanceCm, ActiveSpeedLocked())) {
        FinishCommandLocked(now, false, "obstacle");
        return;
    }
    if (moving_) {
        if (command_interface_.IsNavigation() && !command_interface_.IsNavigationTurn()) {
            const auto imu = GetMpu6050Status();
            heading_target_degrees_ = imu.heading_degrees +
                                      command_interface_.NavigationHeadingErrorRad() * 180.0 /
                                          std::numbers::pi;
        }
        if (!UpdateHeadingPdLocked(now)) {
            StopFailedMotionLocked(now, heading_fault_);
            return;
        }
        if (heading_turn_complete_) {
            if (command_interface_.IsNavigationTurn()) {
                StopOutputsLocked();
                if (fault_ != ESP_OK) {
                    FinishCommandLocked(now, false, "motor_error");
                    return;
                }
                command_interface_.BeginNavigationDrive(now);
                if (!PrepareMotionLocked(Motion::kForward)) {
                    FinishCommandLocked(now, false, "motor_error");
                    return;
                }
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
                start_at_us_ = now + FC03_COAST_MS * 1000LL;
#endif
                heading_hold_active_ = true;
                const auto imu = GetMpu6050Status();
                heading_target_degrees_ = imu.heading_degrees +
                                          command_interface_.NavigationHeadingErrorRad() * 180.0 /
                                              std::numbers::pi;
            } else {
                FinishCommandLocked(now, true);
            }
            return;
        }
        if (!UpdateSpeedPiLocked(now)) {
            StopFailedMotionLocked(now, speed_pi_fault_);
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
