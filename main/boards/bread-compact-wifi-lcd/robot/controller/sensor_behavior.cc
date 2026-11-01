#include "controller_internal.h"

namespace ruby_robot {

void L298nController::UpdateDistance(int distance_cm, bool valid) {
    std::lock_guard lock(mutex_);
    const int64_t now = esp_timer_get_time();
    last_sensor_update_us_ = now;
    sensor_valid_ = valid;
    measured_distance_cm_ = valid ? distance_cm : -1;
    if (!valid) {
        valid_sensor_samples_ = 0;
        avoid_clear_samples_ = 0;
        if (++invalid_sensor_samples_ >= 2 && autonomy_mode_ != AutonomyMode::kManual) {
            if (autonomy_mode_ == AutonomyMode::kObstacleAvoidance &&
                avoid_state_ != AvoidState::kForward && avoid_state_ != AvoidState::kBlocked)
                BlockAvoidLocked();
            StopLocked();
        }
        return;
    }
    invalid_sensor_samples_ = 0;
    valid_sensor_samples_ = std::min(valid_sensor_samples_ + 1, 3);
    if (autonomy_mode_ == AutonomyMode::kManual || web_manual_ || fault_ != ESP_OK ||
        !MotionAllowed()) {
        return;
    }
    // One missed echo must not cancel a live avoidance phase on the first recovered reading.
    // A phase can only start after two valid samples. Two missed echoes or an
    // expired safety deadline still cancel it.
    const bool recovering_avoid_phase = autonomy_mode_ == AutonomyMode::kObstacleAvoidance &&
                                        AvoidMotionPhaseLocked() && motion_ != Motion::kStop &&
                                        esp_timer_get_time() < deadline_us_;
    if (valid_sensor_samples_ < 2 && !recovering_avoid_phase) {
        if (autonomy_mode_ == AutonomyMode::kObstacleAvoidance &&
            avoid_state_ != AvoidState::kForward && avoid_state_ != AvoidState::kBlocked)
            BlockAvoidLocked();
        StopLocked();
        return;
    }
    if (autonomy_mode_ == AutonomyMode::kFollow)
        UpdateFollowLocked(distance_cm);
    else
        UpdateObstacleAvoidanceLocked(distance_cm);
}

void L298nController::ScheduleProximityWarning() {
    bool enqueue = false;
    {
        std::lock_guard lock(mutex_);
        const int64_t now = esp_timer_get_time();
        const int warning_distance =
            ScaleDistanceForSpeed(obstacle_distance_cm_, web_speed_percent_);
        if (web_manual_ && proximity_warning_enabled_ && web_connected_ && web_session_ != 0 &&
            now < web_deadline_us_ && sensor_valid_ &&
            now - last_sensor_update_us_ <= kSensorStaleMs * 1000LL &&
            motion_ == Motion::kForward && measured_distance_cm_ < warning_distance &&
            !warning_queued_ && now >= next_warning_at_us_) {
            warning_queued_ = true;
            enqueue = true;
        }
    }
    if (!enqueue)
        return;
    Application::GetInstance().Schedule([this]() {
        bool warn = false;
        {
            std::lock_guard lock(mutex_);
            const int64_t now = esp_timer_get_time();
            warning_queued_ = false;
            next_warning_at_us_ = now + kProximityWarningPeriodUs;
            const int warning_distance =
                ScaleDistanceForSpeed(obstacle_distance_cm_, web_speed_percent_);
            warn = web_manual_ && proximity_warning_enabled_ && web_connected_ &&
                   web_session_ != 0 && now < web_deadline_us_ && sensor_valid_ &&
                   now - last_sensor_update_us_ <= kSensorStaleMs * 1000LL &&
                   motion_ == Motion::kForward && measured_distance_cm_ < warning_distance;
        }
        auto& app = Application::GetInstance();
        if (warn && app.CanEnterSleepMode())
            app.PlaySound(Lang::Sounds::OGG_POPUP);
    });
}

}  // namespace ruby_robot
