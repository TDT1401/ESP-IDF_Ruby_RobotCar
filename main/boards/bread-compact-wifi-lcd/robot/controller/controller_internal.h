#pragma once

#include "l298n_controller.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_err.h>
#include <esp_pm.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "../../config.h"
#include "../sensors/hcsr04_sensor.h"
#include "../sensors/mpu6050_imu.h"
#include "application.h"
#include "assets/lang_config.h"
#include "mcp_server.h"

namespace ruby_robot {

constexpr auto kMode = LEDC_LOW_SPEED_MODE;
constexpr auto kTimer = LEDC_TIMER_1;  // LCD backlight owns timer/channel 0.
constexpr auto kLeftChannel = LEDC_CHANNEL_1;
constexpr auto kRightChannel = LEDC_CHANNEL_2;
constexpr int kDefaultWebSpeedPercent = 75;
constexpr int64_t kDirectionDelayUs = 50000;
constexpr int64_t kTickUs = 10000;
constexpr int kWebTimeoutMs = 600;
constexpr int kDefaultFollowDistanceCm = 30;
constexpr int kDefaultObstacleDistanceCm = 20;
constexpr int kSensorStaleMs = 250;
constexpr int kReferenceSpeedPercent = 50;
constexpr int64_t kProximityWarningPeriodUs = 700000;

inline int ScaleDistanceForSpeed(int distance_cm, int speed_percent) {
    const int scaled_speed = std::max(speed_percent, kReferenceSpeedPercent);
    return (distance_cm * scaled_speed + kReferenceSpeedPercent - 1) / kReferenceSpeedPercent;
}

enum class Motion { kStop, kForward, kBackward, kLeft, kRight };

inline bool IsSleepEmotion(const char* emotion) {
    return emotion != nullptr && (strcmp(emotion, "sleep") == 0 || strcmp(emotion, "sleepy") == 0);
}

inline const char* MotionName(Motion motion) {
    switch (motion) {
        case Motion::kForward:
            return "forward";
        case Motion::kBackward:
            return "backward";
        case Motion::kLeft:
            return "turn_left";
        case Motion::kRight:
            return "turn_right";
        default:
            return "stopped";
    }
}

class L298nController {
public:
    static L298nController& Instance();

    void Stop();
    L298nMotorStatus Snapshot();
    void UpdateDistance(int distance_cm, bool valid);
    void ScheduleProximityWarning();
    void SetWebConnected(bool connected);
    esp_err_t BeginWebControl(uint32_t* session);
    esp_err_t WebCommand(uint32_t session, uint32_t sequence, L298nWebCommand command, int speed,
                         bool proximity_warning_enabled, int joystick_x, int joystick_y);
    void UpdateEmotion(const char* emotion, bool speaking);

private:
    // Network callbacks and the timer share this controller. Keep the mutex scope short.
    std::mutex mutex_;
    Fc03Encoder encoder_;
    int applied_left_pwm_percent_ = 0;
    int applied_right_pwm_percent_ = 0;
    esp_timer_handle_t timer_ = nullptr;
#ifdef CONFIG_PM_ENABLE
    esp_pm_lock_handle_t sleep_lock_ = nullptr;
    bool sleep_lock_held_ = false;
    esp_pm_lock_handle_t web_sleep_lock_ = nullptr;
    bool web_sleep_lock_held_ = false;
#endif
    Motion motion_ = Motion::kStop;
    int64_t start_at_us_ = 0;
    int64_t deadline_us_ = 0;
    bool moving_ = false;
    esp_err_t fault_ = ESP_OK;
    bool web_connected_ = false;
    bool web_manual_ = false;
    bool web_joystick_active_ = false;
    int web_left_output_percent_ = 0;
    int web_right_output_percent_ = 0;
    uint32_t web_session_ = 0;
    uint32_t web_sequence_ = 0;
    int64_t web_deadline_us_ = 0;
    int web_speed_percent_ = kDefaultWebSpeedPercent;
    int measured_distance_cm_ = -1;
    int64_t last_sensor_update_us_ = 0;
    bool sensor_valid_ = false;
    bool warning_queued_ = false;
    bool proximity_warning_enabled_ = true;
    int64_t next_warning_at_us_ = 0;

    L298nController();
    ~L298nController();
    bool Check(esp_err_t error);
    void ReleaseWebSleepLockLocked();
    void EndWebControlLocked();
    void StopLocked();
    int ActiveSpeedLocked() const;
    bool PrepareMotionLocked(Motion motion);
    static bool MotionAllowed();
    void Tick();
    bool ApplySpeedLocked();
    void StopOutputsLocked();
    bool SetWheelPwmLocked(ledc_channel_t channel, int percent);
    bool SetWheelDirectionLocked(gpio_num_t in1, gpio_num_t in2, int sign, bool inverted);
    bool ApplyMotorPwmLocked(int left_percent, int right_percent);
    int CurrentPwmPercentLocked(int side) const;
    int CurrentLeftPwmPercentLocked() const;
    int CurrentRightPwmPercentLocked() const;
    bool ApplyWebDriveLocked(int left, int right);
    bool ApplyDirectionLocked();
    ToolResult GetState();
    void RegisterTools();
};

}  // namespace ruby_robot
