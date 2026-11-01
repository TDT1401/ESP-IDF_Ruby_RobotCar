#include "controller_internal.h"

#include "application.h"
#include "assets/lang_config.h"

namespace ruby_robot {

void L298nController::UpdateDistance(int distance_cm, bool valid) {
    std::lock_guard lock(mutex_);
    last_sensor_update_us_ = esp_timer_get_time();
    sensor_valid_ = valid;
    measured_distance_cm_ = valid ? distance_cm : -1;
}

void L298nController::ScheduleProximityWarning() {
    bool enqueue = false;
    {
        std::lock_guard lock(mutex_);
        const int64_t now = esp_timer_get_time();
        const int warning_distance =
            ScaleDistanceForSpeed(kDefaultObstacleDistanceCm, web_speed_percent_);
        if (web_control_.active() && proximity_warning_enabled_ && web_control_.connected() &&
            !web_control_.Expired(now) && sensor_valid_ &&
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
                ScaleDistanceForSpeed(kDefaultObstacleDistanceCm, web_speed_percent_);
            warn = web_control_.active() && proximity_warning_enabled_ &&
                   web_control_.connected() && !web_control_.Expired(now) && sensor_valid_ &&
                   now - last_sensor_update_us_ <= kSensorStaleMs * 1000LL &&
                   motion_ == Motion::kForward && measured_distance_cm_ < warning_distance;
        }
        auto& app = Application::GetInstance();
        if (warn && app.CanEnterSleepMode())
            app.PlaySound(Lang::Sounds::OGG_POPUP);
    });
}

}  // namespace ruby_robot
