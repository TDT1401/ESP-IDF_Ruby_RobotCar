#include "controller_internal.h"

namespace ruby_robot {

esp_err_t L298nController::SetFollowMode(bool enabled) {
    std::lock_guard lock(mutex_);
    return SetAutonomyModeLocked(enabled ? AutonomyMode::kFollow : AutonomyMode::kManual,
                                 AutonomyMode::kFollow);
}

esp_err_t L298nController::SetFollowDistance(int distance_cm) {
    if (distance_cm < kMinFollowDistanceCm || distance_cm > kMaxFollowDistanceCm)
        return ESP_ERR_INVALID_ARG;
    std::lock_guard lock(mutex_);
    follow_distance_cm_ = distance_cm;
    return ESP_OK;
}

void L298nController::UpdateFollowLocked(int distance_cm) {
    avoid_state_ = AvoidState::kForward;
    const auto desired = robot_autonomy::DecideFollowMotion(
        distance_cm, follow_distance_cm_, motion_ == Motion::kBackward, kFollowMaxTargetCm,
        kFollowHysteresisCm, kFollowReverseTriggerCm);
    if (desired == Motion::kStop)
        StopLocked();
    else
        RefreshAutonomyMotionLocked(desired, kFollowSpeedPercent);
}

}  // namespace ruby_robot
