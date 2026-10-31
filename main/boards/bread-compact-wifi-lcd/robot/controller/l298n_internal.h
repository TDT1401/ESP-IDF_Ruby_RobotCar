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
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "../../config.h"
#include "../sensors/hcsr04_sensor.h"
#include "../sensors/mpu6050_imu.h"
#include "application.h"
#include "assets/lang_config.h"
#include "autonomy_decisions.h"
#include "mcp_server.h"

namespace ruby_robot {

constexpr auto kMode = LEDC_LOW_SPEED_MODE;
constexpr auto kTimer = LEDC_TIMER_1;  // LCD backlight owns timer/channel 0.
constexpr auto kLeftChannel = LEDC_CHANNEL_1;
constexpr auto kRightChannel = LEDC_CHANNEL_2;
// Straight motions start at the calibrated baseline. Turns use their own PWM;
// the speed PI adjusts straight travel and distance moves use their own ramp.
constexpr int kDefaultSpeedPercent = 50;
constexpr int kTurnSpeedPercent = 75;
// Keep the browser HUD's existing default; it is intentionally separate from
// autonomous and spoken motor commands.
constexpr int kDefaultWebSpeedPercent = 75;
constexpr int kTurnSafetyTimeoutMs = 10000;
// Allow room-scale travel in one encoder-controlled command while keeping each
// autonomous move bounded. At the conservative 20 cm/s timeout rate, 10 m gets
// a 53 s deadline including startup/stopping margin.
constexpr int kMaxDistanceMoveCm = 1000;
constexpr int kDistanceMoveTimeoutMarginMs = 3000;
constexpr int kDistanceMoveTimeoutPerCmMs = 50;
constexpr int kDistanceMinimumDrivePercent = 50;
constexpr int kDistanceCrawlSpeedPercent = 45;
constexpr int kDistanceRampStartSpeedPercent = 50;
constexpr int kDistanceRampLengthCm = 8;
constexpr int64_t kDistanceRampUpdateUs = 50000;
constexpr int64_t kDistanceBrakeUs = 100000;
constexpr int kSpeedPiMinimumPwmPercent = 45;
constexpr int kSpeedPiStartupPwmPercent = 50;
constexpr int64_t kSpeedPiStartupUs = 300000;
constexpr int64_t kSpeedPiUpdateUs = 50000;
constexpr int64_t kHeadingControlUpdateUs = 50000;
constexpr int64_t kImuStaleUs = 100000;
constexpr double kHeadingCorrectionTimeSeconds = 1.5;
constexpr double kTurningCorrectionTimeSeconds = 8.0;
constexpr double kHeadingKp = 0.7;   // PWM percent per heading degree.
constexpr double kHeadingKd = 0.08;  // PWM percent per degree/second.
constexpr int kHeadingMaxCorrectionPercent = 15;
constexpr int kTurnMinimumPwmPercent = 55;
constexpr double kTurnSlowdownWindowDegrees = 25.0;
constexpr double kTurnKp = 1.0;
constexpr double kTurnKd = 0.12;
constexpr double kTurnStopLeadSeconds = 0.025;
constexpr double kTurnStopLeadMaxDegrees = 4.0;
constexpr double kSpeedPiKp = 0.12;
constexpr double kSpeedPiKi = 0.04;
constexpr double kSpeedPiIntegralLimit = 300.0;
constexpr int64_t kDirectionDelayUs = 50000;
constexpr int64_t kTickUs = 10000;
constexpr int kWebTimeoutMs = 600;
constexpr int kDefaultFollowDistanceCm = 30;
constexpr int kFollowSpeedPercent = 70;
constexpr int kDefaultObstacleDistanceCm = 20;
constexpr int kMinFollowDistanceCm = 10;
constexpr int kMaxFollowDistanceCm = 200;
constexpr int kMinObstacleDistanceCm = 10;
constexpr int kMaxObstacleDistanceCm = 100;
constexpr int kFollowMaxTargetCm = 300;
constexpr int kFollowHysteresisCm = 4;
constexpr int kFollowReverseTriggerCm = 10;
constexpr int kAvoidClearanceCm = 10;
constexpr int kAvoidFirstClearCm = 40;
constexpr int kAvoidReverseCm = 6;
constexpr int kAvoidSettleMs = 200;  // Flush the SR04 median filter after stopping a turn.
constexpr int kSensorStaleMs = 250;
constexpr int kReferenceSpeedPercent = 50;
constexpr int64_t kProximityWarningPeriodUs = 700000;
constexpr int kAutonomySpeedPercent = 40;
constexpr int kRouteDriveSpeedPercent = 60;
constexpr int kRouteMaxSteps = 5;
constexpr int kRouteMaxStepCm = 50;
constexpr int64_t kRouteMaxUs = 60000000;
constexpr int kDefaultTurnAngleDegrees = 90;
constexpr int kMaxTurnAngleDegrees = 180;
constexpr double kPi = 3.14159265358979323846;

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
constexpr double kLeftRpmAtPwm50 = FC03_LEFT_RPM_AT_PWM_50;
constexpr double kLeftRpmAtPwm70 = FC03_LEFT_RPM_AT_PWM_70;
constexpr double kRightRpmAtPwm50 = FC03_RIGHT_RPM_AT_PWM_50;
constexpr double kRightRpmAtPwm70 = FC03_RIGHT_RPM_AT_PWM_70;
constexpr bool kSpeedPiCalibrationValid =
    kLeftRpmAtPwm50 > 0 && kLeftRpmAtPwm70 > kLeftRpmAtPwm50&& kRightRpmAtPwm50 > 0 &&
    kRightRpmAtPwm70 > kRightRpmAtPwm50&& FC03_LEFT_PULSES_PER_REV > 0 &&
    FC03_RIGHT_PULSES_PER_REV > 0 && FC03_LEFT_WHEEL_DIAMETER_MM > 0 &&
    FC03_RIGHT_WHEEL_DIAMETER_MM > 0;
static_assert(ROBOT_TRACK_WIDTH_MM > 0);
#else
constexpr double kLeftRpmAtPwm50 = 0;
constexpr double kLeftRpmAtPwm70 = 0;
constexpr double kRightRpmAtPwm50 = 0;
constexpr double kRightRpmAtPwm70 = 0;
constexpr bool kSpeedPiCalibrationValid = false;
#endif

inline double CalibratedRpmAtPwm(double pwm, double rpm_at_50, double rpm_at_70) {
    return rpm_at_50 + (pwm - 50.0) * (rpm_at_70 - rpm_at_50) / 20.0;
}

inline double CalibratedPwmForRpm(double rpm, double rpm_at_50, double rpm_at_70) {
    return 50.0 + (rpm - rpm_at_50) * 20.0 / (rpm_at_70 - rpm_at_50);
}

inline int ScaleDistanceForSpeed(int distance_cm, int speed_percent) {
    // Keep the configured distance as the minimum threshold up to 50% PWM, then
    // increase it proportionally. For example, 20 cm at 50% becomes 30 cm at 75%.
    const int scaled_speed = std::max(speed_percent, kReferenceSpeedPercent);
    return (distance_cm * scaled_speed + kReferenceSpeedPercent - 1) / kReferenceSpeedPercent;
}

inline int DistanceMoveTimeoutMs(int distance_cm) {
    const int estimated_timeout_ms =
        kDistanceMoveTimeoutMarginMs + distance_cm * kDistanceMoveTimeoutPerCmMs;
    return std::max(kTurnSafetyTimeoutMs, estimated_timeout_ms);
}

using Motion = robot_autonomy::Motion;
enum class AutonomyMode { kManual, kFollow, kObstacleAvoidance };
enum class DistanceMoveState {
    kIdle,
    kPending,
    kMoving,
    kCompleted,
    kInterrupted,
    kTimeout,
    kNoPulses,
    kEncoderError,
    kObstacle
};
using AvoidState = robot_autonomy::AvoidPhase;
enum class RouteState { kEmpty, kSaved, kRunning, kComplete, kInterrupted };

inline const char* RouteStateName(RouteState state) {
    switch (state) {
        case RouteState::kSaved:
            return "saved";
        case RouteState::kRunning:
            return "running";
        case RouteState::kComplete:
            return "complete";
        case RouteState::kInterrupted:
            return "interrupted";
        default:
            return "empty";
    }
}

struct RouteStep {
    Motion motion;
    int value;  // Centimetres for F/B, degrees for L/R.
};

inline bool ParseRoute(const char* script, std::array<RouteStep, kRouteMaxSteps>& steps, int& count,
                       std::array<char, 64>& normalized) {
    if (script == nullptr)
        return false;
    size_t length = 0;
    while (length < normalized.size() && script[length] != '\0')
        ++length;
    if (length == 0 || length >= normalized.size())
        return false;
    count = 0;
    size_t written = 0;
    const char* cursor = script;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t')
            ++cursor;
        const char code = static_cast<char>(std::toupper(static_cast<unsigned char>(*cursor++)));
        if (code == 'S') {
            while (*cursor == ' ' || *cursor == '\t')
                ++cursor;
            if (*cursor != '\0' || count == 0)
                return false;
            if (written + 3 > normalized.size())
                return false;
            normalized[written++] = ',';
            normalized[written++] = 'S';
            break;
        }
        Motion motion;
        switch (code) {
            case 'F':
                motion = Motion::kForward;
                break;
            case 'B':
                motion = Motion::kBackward;
                break;
            case 'L':
                motion = Motion::kLeft;
                break;
            case 'R':
                motion = Motion::kRight;
                break;
            default:
                return false;
        }
        if (count >= kRouteMaxSteps || *cursor < '0' || *cursor > '9')
            return false;
        int value = 0;
        while (*cursor >= '0' && *cursor <= '9') {
            value = value * 10 + (*cursor++ - '0');
            if (value > (motion == Motion::kLeft || motion == Motion::kRight ? kMaxTurnAngleDegrees
                                                                             : kRouteMaxStepCm))
                return false;
        }
        if (value == 0)
            return false;
        while (*cursor == ' ' || *cursor == '\t')
            ++cursor;
        char item[8];
        const int size = snprintf(item, sizeof(item), "%s%c%d", count ? "," : "", code, value);
        if (size <= 0 || written + static_cast<size_t>(size) >= normalized.size())
            return false;
        memcpy(normalized.data() + written, item, size);
        written += size;
        steps[count++] = {motion, value};
        if (*cursor == '\0')
            break;
        if (*cursor++ != ',')
            return false;
        if (*cursor == '\0')
            return false;
    }
    if (count == 0)
        return false;
    normalized[written] = '\0';
    return true;
}

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

inline const char* AutonomyModeName(AutonomyMode mode) {
    switch (mode) {
        case AutonomyMode::kFollow:
            return "follow";
        case AutonomyMode::kObstacleAvoidance:
            return "obstacle_avoidance";
        default:
            return "manual";
    }
}

inline const char* DistanceMoveStateName(DistanceMoveState state) {
    switch (state) {
        case DistanceMoveState::kPending:
            return "pending";
        case DistanceMoveState::kMoving:
            return "moving";
        case DistanceMoveState::kCompleted:
            return "completed";
        case DistanceMoveState::kInterrupted:
            return "interrupted";
        case DistanceMoveState::kTimeout:
            return "timeout";
        case DistanceMoveState::kNoPulses:
            return "no_pulses";
        case DistanceMoveState::kEncoderError:
            return "encoder_error";
        case DistanceMoveState::kObstacle:
            return "obstacle";
        default:
            return "idle";
    }
}

class L298nController {
public:
    static L298nController& Instance();

    void Stop();

    esp_err_t SetSpeedPercent(int speed);

    esp_err_t MoveDistanceFromCommand(bool forward, int distance_cm, const char* owner);

    L298nMotorStatus Snapshot();

    esp_err_t SetFollowMode(bool enabled);

    esp_err_t SetObstacleAvoidance(bool enabled);

    esp_err_t SetFollowDistance(int distance_cm);

    esp_err_t SetObstacleDistance(int distance_cm);

    esp_err_t SaveRoute(const char* script);

    esp_err_t RunRoute(const char* owner);

    void CancelRoute();

    void UpdateDistance(int distance_cm, bool valid);

    void ScheduleProximityWarning();

    void SetWebConnected(bool connected);

    esp_err_t BeginWebControl(uint32_t* session);

    esp_err_t WebCommand(uint32_t session, uint32_t sequence, L298nWebCommand command, int speed,
                         bool proximity_warning_enabled, int joystick_x, int joystick_y);

    void UpdateEmotion(const char* emotion, bool speaking);

private:
    // MCP, HTTP and network callbacks and the timer share this controller.
    // Only short state/GPIO operations hold the mutex: no sleeps, JSON, logging,
    // networking or Application::Schedule calls inside it.
    std::mutex mutex_;
    Fc03Encoder encoder_;
    DistanceMoveState distance_move_state_ = DistanceMoveState::kIdle;
    int distance_move_target_cm_ = 0;
    double distance_move_progress_cm_ = 0;
    double distance_move_start_left_m_ = 0;
    double distance_move_start_right_m_ = 0;
    bool angle_turn_ = false;
    int turn_target_degrees_ = 0;
    double turn_progress_degrees_ = 0;
    bool turn_uses_imu_ = false;
    int turn_direction_ = 0;
    double turn_start_yaw_degrees_ = 0;
    int turn_pwm_percent_ = kTurnSpeedPercent;
    bool imu_ready_ = false;
    bool imu_axis_mismatch_ = false;
    double yaw_degrees_ = 0;
    double encoder_yaw_degrees_ = 0;
    double gyro_z_dps_ = 0;
    double last_left_signed_m_ = 0;
    double last_right_signed_m_ = 0;
    int64_t last_imu_sample_us_ = 0;
    int64_t last_heading_update_us_ = 0;
    int64_t last_heading_control_us_ = 0;
    bool heading_hold_active_ = false;
    double heading_target_degrees_ = 0;
    int heading_correction_percent_ = 0;
    int applied_left_pwm_percent_ = 0;
    int applied_right_pwm_percent_ = 0;
    int distance_profile_speed_percent_ = 0;
    int64_t distance_profile_update_us_ = 0;
    bool brake_active_ = false;
    int64_t brake_until_us_ = 0;
    double speed_pi_integral_[2] = {};
    int speed_pi_output_percent_[2] = {};
    double speed_pi_target_rpm_ = 0;
    int64_t speed_pi_startup_until_us_ = 0;
    int64_t speed_pi_last_update_us_ = 0;
    bool speed_pi_active_ = false;
    esp_timer_handle_t timer_ = nullptr;
#ifdef CONFIG_PM_ENABLE
    esp_pm_lock_handle_t sleep_lock_ = nullptr;
    bool sleep_lock_held_ = false;
    esp_pm_lock_handle_t web_sleep_lock_ = nullptr;
    bool web_sleep_lock_held_ = false;
#endif
    Motion motion_ = Motion::kStop;
    int speed_percent_ = kDefaultSpeedPercent;
    int64_t start_at_us_ = 0;
    int64_t deadline_us_ = 0;
    bool moving_ = false;
    esp_err_t fault_ = ESP_OK;
    bool response_active_ = false;
    bool manual_override_ = false;
    bool web_connected_ = false;
    bool web_manual_ = false;
    bool web_joystick_active_ = false;
    int web_left_output_percent_ = 0;
    int web_right_output_percent_ = 0;
    uint32_t web_session_ = 0;
    uint32_t web_sequence_ = 0;
    int64_t web_deadline_us_ = 0;
    int web_speed_percent_ = kDefaultWebSpeedPercent;
    AutonomyMode autonomy_mode_ = AutonomyMode::kManual;
    AvoidState avoid_state_ = AvoidState::kForward;
    int follow_distance_cm_ = kDefaultFollowDistanceCm;
    int obstacle_distance_cm_ = kDefaultObstacleDistanceCm;
    int measured_distance_cm_ = -1;
    int autonomy_speed_percent_ = kAutonomySpeedPercent;
    int valid_sensor_samples_ = 0;
    int invalid_sensor_samples_ = 0;
    int avoid_clear_samples_ = 0;
    int64_t last_sensor_update_us_ = 0;
    int64_t avoid_phase_deadline_us_ = 0;
    int avoid_first_distance_cm_ = -1;
    int avoid_sample_distance_cm_ = -1;
    bool sensor_valid_ = false;
    bool warning_queued_ = false;
    bool proximity_warning_enabled_ = true;
    int64_t next_warning_at_us_ = 0;
    std::array<RouteStep, kRouteMaxSteps> route_steps_ = {};
    std::array<char, 64> route_script_ = {};
    int route_count_ = 0;
    int route_index_ = 0;
    int64_t route_deadline_us_ = 0;
    bool route_active_ = false;
    RouteState route_state_ = RouteState::kEmpty;
    const char* control_owner_ = "none";

    const char* ControlOwnerLocked() const;

    void CancelRouteLocked();

    bool StartRouteStepLocked();

    L298nController();

    ~L298nController();

    bool Check(esp_err_t error);

    void ReleaseWebSleepLockLocked();

    void EndWebControlLocked();

    void DisableAutonomyLocked();

    esp_err_t SetAutonomyModeLocked(AutonomyMode requested, AutonomyMode controlled_mode);

    bool RefreshAutonomyMotionLocked(Motion motion, int speed_percent);

    void UpdateFollowLocked(int distance_cm);

    bool AvoidMotionPhaseLocked() const;

    void BlockAvoidLocked();

    void StartAvoidMotionLocked(AvoidState state, Motion motion, int target);

    void FinishAvoidMotionLocked(int64_t now);

    void DriveAvoidForwardLocked();

    void UpdateObstacleAvoidanceLocked(int distance_cm);

    void StopLocked();

    void UpdateYawEstimateLocked(int64_t now);

    void UpdateYawControlLocked(int64_t now);

    void UpdateDistanceMoveLocked();

    void UpdateDistanceSpeedProfileLocked(int64_t now);

    int ActiveSpeedLocked() const;

    bool PrepareMotionLocked(Motion motion);

    bool PrepareDistanceMoveLocked(Motion motion, int distance_cm);

    bool PrepareAngleTurnLocked(Motion motion, int degrees);

    static bool MotionAllowed();

    void Tick();

    void BeginDistanceBrakeLocked(int64_t now);

    void ResetSpeedPiLocked();

    bool ApplySpeedLocked();

    void UpdateSpeedPiLocked(int64_t now);

    void StopOutputsLocked();

    bool SetWheelPwmLocked(ledc_channel_t channel, int percent);

    bool SetWheelDirectionLocked(gpio_num_t in1, gpio_num_t in2, int sign, bool inverted);

    bool ApplyMotorPwmLocked(int left_percent, int right_percent);

    int CurrentPwmPercentLocked(int side) const;

    int CurrentLeftPwmPercentLocked() const;

    int CurrentRightPwmPercentLocked() const;

    bool ApplyWebDriveLocked(int left, int right);

    bool ApplyDirectionLocked();

    ToolResult MoveAngle(Motion motion, const PropertyList& properties);

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
    ToolResult MoveDistance(Motion motion, const PropertyList& properties);

    esp_err_t StartDistanceMoveCommand(Motion motion, int distance_cm, const char* owner);
#endif

    ToolResult SetSpeed(int speed);

    ToolResult GetState();

    void RegisterTools();
};

}  // namespace ruby_robot
