#include "controller_internal.h"

#include "imu.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ruby_robot {

void MotorController::StopLocked() {
    StopGoalLocked(esp_timer_get_time());
    StopOutputsLocked();
}

void MotorController::FinishCommandLocked(int64_t now_us, bool completed, const char* fault) {
    StopOutputsLocked();
    // Keep the mode until its result is recorded; stopping the goal would turn it idle.
    if (completed && fault_ == ESP_OK)
        CompleteGoalLocked(now_us);
    else
        FailGoalLocked(now_us, fault_ == ESP_OK ? fault : "motor_error");
}

void MotorController::StopFailedMotionLocked(int64_t now_us, const char* fault) {
    if (web_control_.active()) {
        EndWebControlLocked();
        return;
    }
    if (GoalActiveLocked())
        FinishCommandLocked(now_us, false, fault);
    else
        StopLocked();
}

int MotorController::ActiveSpeedLocked() const {
    if (heading_turn_active_) {
        const double reference_rpm =
            std::min(config_.speed_pi_left_reference_rpm, config_.speed_pi_right_reference_rpm);
        if (reference_rpm <= 0)
            return 0;
        const double percent =
            std::abs(heading_correction_rpm_) * config_.speed_pi_reference_pwm_percent / reference_rpm;
        return std::clamp(static_cast<int>(std::lround(percent)), 0, 100);
    }
    return GoalActiveLocked() ? GoalSpeedLocked() : web_speed_percent_;
}

bool MotorController::PrepareMotionLocked(Motion motion) {
#ifdef CONFIG_PM_ENABLE
    if (!Check(esp_pm_lock_acquire(sleep_lock_)))
        return false;
    sleep_lock_held_ = true;
#endif
    motion_ = motion;
    start_at_us_ = esp_timer_get_time() + kDirectionDelayUs;
    return true;
}

bool MotorController::MotionAllowed() const {
    return config_.motion_allowed();
}

void MotorController::Tick() {
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
    if (GoalExpiredLocked(now)) {
        const bool angle_turn = IsAngleTurnLocked();
        if (angle_turn)
            heading_fault_ = "turn_timeout";
        FinishCommandLocked(now, false, angle_turn ? "turn_timeout" : "timeout");
        return;
    }
    if (motion_ == Motion::kStop)
        return;
    const auto encoder = encoder_.Snapshot();
    const auto goal_pose = IsNavigationLocked() ? NavigationPoseLocked() : odometry_.Snapshot();
    if (!UpdateGoalLocked(goal_pose, encoder.left.rpm, encoder.right.rpm, now)) {
        StopFailedMotionLocked(now, GoalFaultLocked());
        return;
    }
    if (GoalCompletionReadyLocked()) {
        FinishCommandLocked(now, true);
        return;
    }
    if (IsNavigationLocked() && sensor_valid_ &&
        now - last_sensor_update_us_ <= kSensorStaleMs * 1000LL &&
        measured_distance_cm_ <=
            ScaleDistanceForSpeed(kDefaultObstacleDistanceCm, ActiveSpeedLocked())) {
        FinishCommandLocked(now, false, "obstacle");
        return;
    }
    if (moving_) {
        if (!UpdateHeadingPdLocked(now)) {
            StopFailedMotionLocked(now, heading_fault_);
            return;
        }
        if (heading_turn_complete_) {
            if (IsNavigationTurnLocked()) {
                StopOutputsLocked();
                if (fault_ != ESP_OK) {
                    FinishCommandLocked(now, false, "motor_error");
                    return;
                }
                navigator_.BeginDrive(now);
                if (!PrepareMotionLocked(Motion::kForward)) {
                    FinishCommandLocked(now, false, "motor_error");
                    return;
                }
                if (config_.encoder.enabled)
                    start_at_us_ = now + config_.encoder.coast_ms * 1000LL;
                heading_hold_active_ = true;
                const auto imu = GetMpu6050Status();
                heading_target_degrees_ =
                    imu.heading_degrees + navigator_.heading_error_rad() * 180.0 / std::numbers::pi;
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

bool MotorController::ApplySpeedLocked() {
    if (heading_hold_active_)
        return UpdateHeadingPdLocked(esp_timer_get_time());
    int speed = 0;
    if (mode_ == CommandMode::kManual) {
        speed = ActiveSpeedLocked();
    } else if (mode_ == CommandMode::kDistanceDrive) {
        speed = distance_drive_.speed_percent();
        if (distance_drive_.direction() == Motion::kBackward)
            speed = -speed;
    }
    return SetRequestedWheelsLocked(speed, speed);
}

bool MotorController::ValidAngleTurn(const AngleTurnRequest& request) {
    const int angle = request.angle_degrees;
    return ((angle >= kMinAngleDegrees && angle <= kMaxAngleDegrees) ||
            (angle <= -kMinAngleDegrees && angle >= -kMaxAngleDegrees)) &&
           (request.owner == AngleTurnOwner::kWeb || request.owner == AngleTurnOwner::kVoice);
}

AngleTurnSnapshot MotorController::TurnSnapshotLocked() const {
    const char* state = "idle";
    switch (turn_state_) {
        case AngleTurnState::kRunning:
            state = "running";
            break;
        case AngleTurnState::kCompleted:
            state = "completed";
            break;
        case AngleTurnState::kInterrupted:
            state = "interrupted";
            break;
        case AngleTurnState::kFault:
            state = "fault";
            break;
        default:
            break;
    }
    const auto direction = turn_request_.angle_degrees > 0   ? Motion::kLeft
                           : turn_request_.angle_degrees < 0 ? Motion::kRight
                                                             : Motion::kStop;
    return {state, turn_request_.owner == AngleTurnOwner::kVoice ? "voice" : "web",
            MotionName(direction), turn_request_.angle_degrees, 0};
}

}  // namespace ruby_robot
