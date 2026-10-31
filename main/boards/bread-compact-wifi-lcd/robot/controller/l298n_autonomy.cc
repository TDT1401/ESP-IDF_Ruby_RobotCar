#include "l298n_internal.h"

namespace ruby_robot {

void L298nController::DisableAutonomyLocked() {
    autonomy_mode_ = AutonomyMode::kManual;
    avoid_state_ = AvoidState::kForward;
    avoid_clear_samples_ = 0;
    avoid_phase_deadline_us_ = 0;
    avoid_first_distance_cm_ = -1;
    avoid_sample_distance_cm_ = -1;
    valid_sensor_samples_ = 0;
    invalid_sensor_samples_ = 0;
}

esp_err_t L298nController::SetAutonomyModeLocked(AutonomyMode requested,
                                                 AutonomyMode controlled_mode) {
    if (requested == AutonomyMode::kManual && autonomy_mode_ != controlled_mode)
        return ESP_OK;
    if (web_manual_)
        return ESP_ERR_INVALID_STATE;
    if (requested != AutonomyMode::kManual &&
        (!MotionAllowed() || fault_ != ESP_OK || speed_percent_ == 0)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (requested != AutonomyMode::kManual) {
        const auto encoder = encoder_.Snapshot();
        if (!encoder.ready || !encoder.calibrated || !imu_ready_)
            return ESP_ERR_INVALID_STATE;
    }
    StopLocked();
    manual_override_ = requested != AutonomyMode::kManual;
    DisableAutonomyLocked();
    autonomy_mode_ = requested;
    return ESP_OK;
}

bool L298nController::RefreshAutonomyMotionLocked(Motion motion, int speed_percent) {
    autonomy_speed_percent_ = std::min(speed_percent_, speed_percent);
    if (autonomy_speed_percent_ <= 0) {
        StopLocked();
        return true;
    }
    if (motion_ != motion) {
        StopLocked();
        return fault_ == ESP_OK && PrepareMotionLocked(motion);
    }
    if (moving_ && !ApplySpeedLocked()) {
        StopLocked();
        return false;
    }
    return true;
}

}  // namespace ruby_robot
