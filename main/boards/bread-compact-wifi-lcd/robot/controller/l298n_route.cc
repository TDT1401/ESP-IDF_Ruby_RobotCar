#include "l298n_internal.h"

namespace ruby_robot {

esp_err_t L298nController::SaveRoute(const char* script) {
    std::array<RouteStep, kRouteMaxSteps> steps = {};
    std::array<char, 64> normalized = {};
    int count = 0;
    if (!ParseRoute(script, steps, count, normalized))
        return ESP_ERR_INVALID_ARG;
    std::lock_guard lock(mutex_);
    if (route_active_)
        return ESP_ERR_INVALID_STATE;
    route_steps_ = steps;
    route_count_ = count;
    route_script_ = normalized;
    route_state_ = RouteState::kSaved;
    return ESP_OK;
}

esp_err_t L298nController::RunRoute(const char* owner) {
#ifndef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
    (void)owner;
    return ESP_ERR_NOT_SUPPORTED;
#else
    std::lock_guard lock(mutex_);
    const auto encoder = encoder_.Snapshot();
    if (route_count_ == 0 || route_active_ || web_manual_ ||
        (owner != nullptr && strcmp(owner, "web") == 0 && !web_connected_) ||
        autonomy_mode_ != AutonomyMode::kManual || motion_ != Motion::kStop || brake_active_ ||
        response_active_ || !encoder.ready || !encoder.calibrated || !imu_ready_ ||
        fault_ != ESP_OK || speed_percent_ == 0 || !MotionAllowed())
        return ESP_ERR_INVALID_STATE;
    manual_override_ = true;
    route_active_ = true;
    route_state_ = RouteState::kRunning;
    route_index_ = 0;
    route_deadline_us_ = esp_timer_get_time() + kRouteMaxUs;
    control_owner_ = owner != nullptr && strcmp(owner, "telegram") == 0 ? "telegram"
                     : owner != nullptr && strcmp(owner, "web") == 0    ? "web"
                                                                        : "voice";
    if (!StartRouteStepLocked()) {
        CancelRouteLocked();
        return ESP_FAIL;
    }
    return ESP_OK;
#endif
}

void L298nController::CancelRoute() {
    std::lock_guard lock(mutex_);
    CancelRouteLocked();
}

void L298nController::CancelRouteLocked() {
    if (!route_active_)
        return;
    route_active_ = false;
    route_state_ = RouteState::kInterrupted;
    StopLocked();
    manual_override_ = response_active_;
}

bool L298nController::StartRouteStepLocked() {
    if (route_index_ >= route_count_)
        return false;
    const auto& step = route_steps_[route_index_];
    if (step.motion == Motion::kForward &&
        (!sensor_valid_ ||
         esp_timer_get_time() - last_sensor_update_us_ > kSensorStaleMs * 1000LL ||
         measured_distance_cm_ <= ScaleDistanceForSpeed(obstacle_distance_cm_, speed_percent_)))
        return false;
    StopLocked();
    if (step.motion == Motion::kLeft || step.motion == Motion::kRight)
        return PrepareAngleTurnLocked(step.motion, step.value);
    if (!PrepareDistanceMoveLocked(step.motion, step.value))
        return false;
    return true;
}

}  // namespace ruby_robot
