#include "controller_internal.h"

namespace ruby_robot {

void MotorController::UpdateDistance(int distance_cm, bool valid) {
    std::lock_guard lock(mutex_);
    last_sensor_update_us_ = esp_timer_get_time();
    sensor_valid_ = valid;
    measured_distance_cm_ = valid ? distance_cm : -1;
}

bool MotorController::ProximityWarningAllowedLocked(int64_t now_us) const {
    return web_control_.active() && proximity_warning_enabled_ && web_control_.connected() &&
           !web_control_.Expired(now_us) && sensor_valid_ &&
           now_us - last_sensor_update_us_ <= kSensorStaleMs * 1000LL &&
           motion_ == Motion::kForward &&
           measured_distance_cm_ <
               ScaleDistanceForSpeed(kDefaultObstacleDistanceCm, web_speed_percent_);
}

void MotorController::ScheduleProximityWarning() {
    bool enqueue = false;
    {
        std::lock_guard lock(mutex_);
        const int64_t now = esp_timer_get_time();
        if (ProximityWarningAllowedLocked(now) && !warning_queued_ &&
            now >= next_warning_at_us_) {
            warning_queued_ = true;
            enqueue = true;
        }
    }
    if (!enqueue)
        return;
    config_.schedule([this]() {
        bool warn = false;
        {
            std::lock_guard lock(mutex_);
            const int64_t now = esp_timer_get_time();
            warning_queued_ = false;
            next_warning_at_us_ = now + kProximityWarningPeriodUs;
            warn = ProximityWarningAllowedLocked(now);
        }
        if (warn)
            config_.play_proximity_warning();
    });
}

}  // namespace ruby_robot
