#pragma once

#include "../commands/command_interface.h"
#include "../domain/motion_policy.h"
#include "../domain/web_control_session.h"
#include "../drivers/l298n_motor_driver.h"
#include "heading_pd.h"
#include "l298n_controller.h"
#include "speed_pi.h"

#include <esp_err.h>
#include <esp_pm.h>
#include <esp_timer.h>

#include <cstdint>
#include <mutex>

namespace ruby_robot {

constexpr int kDefaultWebSpeedPercent = 75;
constexpr int64_t kDirectionDelayUs = 50000;
constexpr int64_t kTickUs = 10000;
constexpr int64_t kSpeedPiPeriodUs = 100000;
constexpr int64_t kHeadingPdPeriodUs = 50000;
constexpr int64_t kTurnSettleUs = 150000;
constexpr int kDefaultFollowDistanceCm = 30;
constexpr int kDefaultObstacleDistanceCm = 20;
constexpr int kSensorStaleMs = 250;
constexpr int64_t kProximityWarningPeriodUs = 700000;

class L298nController {
public:
    static L298nController& Instance();

    void Stop();
    esp_err_t StartCommand(const CommandRequest& request);
    esp_err_t ResetOdometry();
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
    L298nMotorDriver driver_;
    CommandInterface command_interface_;
    WebControlSession web_control_;
    Fc03Encoder encoder_;
    Odometry odometry_;
    int64_t last_motor_stop_us_ = 0;
    SpeedPi left_speed_pi_;
    SpeedPi right_speed_pi_;
    int requested_left_percent_ = 0;
    int requested_right_percent_ = 0;
    int64_t last_speed_pi_us_ = 0;
    const char* speed_pi_fault_ = "none";
    bool heading_hold_active_ = false;
    bool heading_turn_active_ = false;
    bool heading_turn_complete_ = false;
    double heading_turn_start_degrees_ = 0;
    double turn_progress_degrees_ = 0;
    int64_t turn_settled_since_us_ = 0;
    double heading_target_degrees_ = 0;
    double heading_error_degrees_ = 0;
    double heading_correction_rpm_ = 0;
    int64_t last_heading_pd_us_ = 0;
    const char* heading_fault_ = "none";
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
    bool moving_ = false;
    esp_err_t fault_ = ESP_OK;
    bool web_joystick_active_ = false;
    bool web_joystick_pending_ = false;
    int web_left_output_percent_ = 0;
    int web_right_output_percent_ = 0;
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
    void StopFailedMotionLocked(int64_t now_us, const char* fault = "control_error");
    void FinishCommandLocked(int64_t now_us, bool completed, const char* fault = "control_error");
    OdometrySample ReadOdometrySampleLocked(int64_t now_us);
    void UpdateOdometryLocked(int64_t now_us);
    bool OdometryStationaryLocked(int64_t now_us) const;
    static DistanceDriveLimits DistanceLimits();
    int ActiveSpeedLocked() const;
    bool PrepareMotionLocked(Motion motion);
    static bool MotionAllowed();
    void Tick();
    bool ApplySpeedLocked();
    bool ApplyCommandLocked();
    void StopOutputsLocked();
    bool ApplyMotorPwmLocked(int left_percent, int right_percent);
    bool SetRequestedWheelsLocked(int left_percent, int right_percent);
    bool UpdateSpeedPiLocked(int64_t now_us);
    bool UpdateHeadingPdLocked(int64_t now_us);
    bool SpeedPiConfiguredLocked() const;
    bool SpeedPiReadyLocked() const;
    bool ApplyWebDriveLocked(int left, int right);
    bool ApplyDirectionLocked();
};

}  // namespace ruby_robot
