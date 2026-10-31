#include "l298n_internal.h"

namespace ruby_robot {

esp_err_t L298nController::SetObstacleAvoidance(bool enabled) {
    std::lock_guard lock(mutex_);
    return SetAutonomyModeLocked(enabled ? AutonomyMode::kObstacleAvoidance : AutonomyMode::kManual,
                                 AutonomyMode::kObstacleAvoidance);
}

esp_err_t L298nController::SetObstacleDistance(int distance_cm) {
    if (distance_cm < kMinObstacleDistanceCm || distance_cm > kMaxObstacleDistanceCm)
        return ESP_ERR_INVALID_ARG;
    std::lock_guard lock(mutex_);
    obstacle_distance_cm_ = distance_cm;
    return ESP_OK;
}

bool L298nController::AvoidMotionPhaseLocked() const {
    return avoid_state_ == AvoidState::kReverse || avoid_state_ == AvoidState::kTurnFirst ||
           avoid_state_ == AvoidState::kTurnSecond || avoid_state_ == AvoidState::kReturnFirst;
}

void L298nController::BlockAvoidLocked() {
    StopLocked();
    avoid_state_ = AvoidState::kBlocked;
    avoid_clear_samples_ = 0;
    avoid_phase_deadline_us_ = 0;
    avoid_first_distance_cm_ = -1;
    avoid_sample_distance_cm_ = -1;
}

void L298nController::StartAvoidMotionLocked(AvoidState state, Motion motion, int target) {
    StopLocked();
    avoid_state_ = state;
    avoid_clear_samples_ = 0;
    avoid_phase_deadline_us_ = 0;
    const bool started = state == AvoidState::kReverse ? PrepareDistanceMoveLocked(motion, target)
                                                       : PrepareAngleTurnLocked(motion, target);
    if (!started)
        BlockAvoidLocked();
}

void L298nController::FinishAvoidMotionLocked(int64_t now) {
    StopLocked();
    if (avoid_state_ == AvoidState::kReverse) {
        StartAvoidMotionLocked(AvoidState::kTurnFirst, Motion::kLeft, kDefaultTurnAngleDegrees);
        return;
    }
    if (avoid_state_ == AvoidState::kTurnFirst)
        avoid_state_ = AvoidState::kMeasureFirst;
    else if (avoid_state_ == AvoidState::kTurnSecond)
        avoid_state_ = AvoidState::kMeasureSecond;
    else
        avoid_state_ = AvoidState::kMeasureChosen;
    avoid_phase_deadline_us_ = now + kAvoidSettleMs * 1000LL;
    avoid_clear_samples_ = 0;
    avoid_sample_distance_cm_ = -1;
}

void L298nController::DriveAvoidForwardLocked() {
    avoid_state_ = AvoidState::kForward;
    avoid_phase_deadline_us_ = 0;
    avoid_clear_samples_ = 0;
    if (!RefreshAutonomyMotionLocked(Motion::kForward, kAutonomySpeedPercent))
        BlockAvoidLocked();
}

void L298nController::UpdateObstacleAvoidanceLocked(int distance_cm) {
    const int stop_distance = ScaleDistanceForSpeed(obstacle_distance_cm_, speed_percent_);
    const int clear_distance = stop_distance + kAvoidClearanceCm;
    const int64_t now = esp_timer_get_time();
    if (AvoidMotionPhaseLocked()) {
        if (distance_move_state_ == DistanceMoveState::kCompleted && !brake_active_)
            FinishAvoidMotionLocked(now);
        else if (distance_move_state_ != DistanceMoveState::kPending &&
                 distance_move_state_ != DistanceMoveState::kMoving &&
                 distance_move_state_ != DistanceMoveState::kCompleted)
            BlockAvoidLocked();
        return;
    }

    int measured = distance_cm;
    if (avoid_state_ == AvoidState::kBlocked) {
        avoid_clear_samples_ = distance_cm > clear_distance ? avoid_clear_samples_ + 1 : 0;
    } else if (avoid_state_ != AvoidState::kForward) {
        // Probe only after old-heading samples leave the sensor filter.
        if (now < avoid_phase_deadline_us_)
            return;
        avoid_sample_distance_cm_ = avoid_clear_samples_ == 0
                                        ? distance_cm
                                        : std::min(avoid_sample_distance_cm_, distance_cm);
        if (++avoid_clear_samples_ < 2)
            return;
        measured = avoid_sample_distance_cm_;
        if (avoid_state_ == AvoidState::kMeasureFirst)
            avoid_first_distance_cm_ = measured;
    }
    const auto action = robot_autonomy::DecideAvoidAction(
        avoid_state_, measured, stop_distance, clear_distance, kAvoidFirstClearCm,
        avoid_clear_samples_, avoid_first_distance_cm_);
    switch (action) {
        case robot_autonomy::AvoidAction::kStop:
            StopLocked();
            return;
        case robot_autonomy::AvoidAction::kForward:
            DriveAvoidForwardLocked();
            return;
        case robot_autonomy::AvoidAction::kReverse:
            avoid_first_distance_cm_ = -1;
            StartAvoidMotionLocked(AvoidState::kReverse, Motion::kBackward, kAvoidReverseCm);
            return;
        case robot_autonomy::AvoidAction::kTurnRight:
            // Cross twice the first turn angle to probe the right side.
            StartAvoidMotionLocked(AvoidState::kTurnSecond, Motion::kRight, kMaxTurnAngleDegrees);
            return;
        case robot_autonomy::AvoidAction::kTurnLeft:
            StartAvoidMotionLocked(AvoidState::kReturnFirst, Motion::kLeft, kMaxTurnAngleDegrees);
            return;
        case robot_autonomy::AvoidAction::kBlock:
            BlockAvoidLocked();
            return;
        case robot_autonomy::AvoidAction::kNone:
            return;
    }
}

}  // namespace ruby_robot
