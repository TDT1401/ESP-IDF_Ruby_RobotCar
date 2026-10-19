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
#include "../../robot_eye_display.h"
#include "../sensors/hcsr04_sensor.h"
#include "../telegram/telegram.h"
#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "mcp_server.h"

namespace {

constexpr auto kMode = LEDC_LOW_SPEED_MODE;
constexpr auto kTimer = LEDC_TIMER_1;  // LCD backlight owns timer/channel 0.
constexpr auto kLeftChannel = LEDC_CHANNEL_1;
constexpr auto kRightChannel = LEDC_CHANNEL_2;
// Non-HUD motions start at the calibrated, conservative baseline. The speed PI
// independently adjusts each wheel, while distance moves apply their own ramp.
constexpr int kDefaultSpeedPercent = 50;
// Keep the browser HUD's existing default; it is intentionally separate from
// autonomous and spoken motor commands.
constexpr int kDefaultWebSpeedPercent = 75;
constexpr int kDefaultDurationMs = 1000;
constexpr int kDefaultTurnDurationMs = 400;
constexpr int kMaxDurationMs = 10000;
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
constexpr double kSpeedPiKp = 0.12;
constexpr double kSpeedPiKi = 0.04;
constexpr double kSpeedPiIntegralLimit = 300.0;
constexpr int64_t kDirectionDelayUs = 50000;
constexpr int64_t kTickUs = 10000;
constexpr int64_t kGestureMaxUs = 2000000;
constexpr int64_t kGestureCooldownUs = 3000000;
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
constexpr int kAutonomyCommandMs = 180;
constexpr int kAvoidFirstClearCm = 40;
constexpr int kAvoidReverseMs = 300;
constexpr int kAvoidSideTurnMs = 550;
constexpr int kAvoidSettleMs = 200;  // Flush the SR04 median filter after stopping a turn.
constexpr int kSensorStaleMs = 250;
constexpr int kReferenceSpeedPercent = 50;
constexpr int64_t kProximityWarningPeriodUs = 700000;
constexpr int kAutonomySpeedPercent = 40;
constexpr int kAvoidReverseSpeedPercent = 45;
constexpr int kAutonomyTurnSpeedPercent = 60;
constexpr int kGuardChangeCm = 20;
constexpr int64_t kGuardCooldownUs = 30000000;
constexpr int kScanTurnMs = 350;
constexpr int64_t kScanSettleUs = 350000;
constexpr int kRouteMaxSteps = 5;
constexpr int kRouteMaxStepCm = 50;
constexpr int64_t kRouteMaxUs = 60000000;

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
constexpr double kLeftRpmAtPwm50 = FC03_LEFT_RPM_AT_PWM_50;
constexpr double kLeftRpmAtPwm70 = FC03_LEFT_RPM_AT_PWM_70;
constexpr double kRightRpmAtPwm50 = FC03_RIGHT_RPM_AT_PWM_50;
constexpr double kRightRpmAtPwm70 = FC03_RIGHT_RPM_AT_PWM_70;
constexpr bool kSpeedPiCalibrationValid =
    kLeftRpmAtPwm50 > 0 && kLeftRpmAtPwm70 > kLeftRpmAtPwm50 && kRightRpmAtPwm50 > 0 &&
    kRightRpmAtPwm70 > kRightRpmAtPwm50 && FC03_LEFT_PULSES_PER_REV > 0 &&
    FC03_RIGHT_PULSES_PER_REV > 0 && FC03_LEFT_WHEEL_DIAMETER_MM > 0 &&
    FC03_RIGHT_WHEEL_DIAMETER_MM > 0;
#else
constexpr double kLeftRpmAtPwm50 = 0;
constexpr double kLeftRpmAtPwm70 = 0;
constexpr double kRightRpmAtPwm50 = 0;
constexpr double kRightRpmAtPwm70 = 0;
constexpr bool kSpeedPiCalibrationValid = false;
#endif

double CalibratedRpmAtPwm(double pwm, double rpm_at_50, double rpm_at_70) {
    return rpm_at_50 + (pwm - 50.0) * (rpm_at_70 - rpm_at_50) / 20.0;
}

double CalibratedPwmForRpm(double rpm, double rpm_at_50, double rpm_at_70) {
    return 50.0 + (rpm - rpm_at_50) * 20.0 / (rpm_at_70 - rpm_at_50);
}

int ScaleDistanceForSpeed(int distance_cm, int speed_percent) {
    // Keep the configured distance as the minimum threshold up to 50% PWM, then
    // increase it proportionally. For example, 20 cm at 50% becomes 30 cm at 75%.
    const int scaled_speed = std::max(speed_percent, kReferenceSpeedPercent);
    return (distance_cm * scaled_speed + kReferenceSpeedPercent - 1) /
           kReferenceSpeedPercent;
}

int DistanceMoveTimeoutMs(int distance_cm) {
    const int estimated_timeout_ms =
        kDistanceMoveTimeoutMarginMs + distance_cm * kDistanceMoveTimeoutPerCmMs;
    return std::max(kMaxDurationMs, estimated_timeout_ms);
}

enum class Motion { kStop, kForward, kBackward, kLeft, kRight };
enum class Emotion { kNone, kHappy, kSad, kAngry, kSurprised, kThinking, kWinking };
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
enum class AvoidState {
    kForward,
    kReverse,
    kTurnFirst,
    kMeasureFirst,
    kTurnSecond,
    kMeasureSecond,
    kReturnFirst,
    kMeasureChosen,
    kBlocked
};
enum class ScanState {
    kIdle,
    kCenter,
    kTurnLeft,
    kLeft,
    kTurnRight,
    kRight,
    kReturn,
    kComplete,
    kInterrupted
};
enum class RouteState { kEmpty, kSaved, kRunning, kComplete, kInterrupted };

const char* RouteStateName(RouteState state) {
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
    int distance_cm;
};

bool ParseRoute(const char* script, std::array<RouteStep, kRouteMaxSteps>& steps, int& count,
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
        int distance = 0;
        while (*cursor >= '0' && *cursor <= '9') {
            distance = distance * 10 + (*cursor++ - '0');
            if (distance > kRouteMaxStepCm)
                return false;
        }
        if (distance == 0)
            return false;
        while (*cursor == ' ' || *cursor == '\t')
            ++cursor;
        char item[8];
        const int size = snprintf(item, sizeof(item), "%s%c%d", count ? "," : "", code, distance);
        if (size <= 0 || written + static_cast<size_t>(size) >= normalized.size())
            return false;
        memcpy(normalized.data() + written, item, size);
        written += size;
        steps[count++] = {motion, distance};
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

const char* ScanStateName(ScanState state) {
    switch (state) {
        case ScanState::kCenter:
        case ScanState::kTurnLeft:
        case ScanState::kLeft:
        case ScanState::kTurnRight:
        case ScanState::kRight:
        case ScanState::kReturn:
            return "scanning";
        case ScanState::kComplete:
            return "complete";
        case ScanState::kInterrupted:
            return "interrupted";
        default:
            return "idle";
    }
}

struct GestureStep {
    Motion motion;
    int duration_ms;
    int speed_percent;
};

// Timed gestures, not position-controlled moves. Each direction change includes a 50 ms stop.
constexpr GestureStep kHappyGesture[] = {{Motion::kForward, 180, 65},
                                         {Motion::kBackward, 180, 65},
                                         {Motion::kLeft, 140, 65},
                                         {Motion::kRight, 280, 65},
                                         {Motion::kLeft, 140, 65}};
constexpr GestureStep kSadGesture[] = {{Motion::kBackward, 180, 40}};
constexpr GestureStep kSurprisedGesture[] = {{Motion::kBackward, 140, 50}};
constexpr GestureStep kAngryGesture[] = {
    {Motion::kLeft, 150, 60}, {Motion::kRight, 250, 60}, {Motion::kLeft, 150, 60}};
constexpr GestureStep kThinkingGesture[] = {
    {Motion::kLeft, 170, 55}, {Motion::kRight, 270, 55}, {Motion::kLeft, 170, 55}};
constexpr GestureStep kWinkingGesture[] = {
    {Motion::kLeft, 200, 60}, {Motion::kRight, 300, 60}, {Motion::kLeft, 200, 60}};

template <size_t N>
constexpr bool IsValidGesture(const GestureStep (&steps)[N]) {
    int64_t total_us = 0;
    for (const auto& step : steps) {
        if (step.motion == Motion::kStop || step.duration_ms < 100 || step.duration_ms > 400 ||
            step.speed_percent <= 0 || step.speed_percent > 65)
            return false;
        total_us += static_cast<int64_t>(step.duration_ms) * 1000 + kDirectionDelayUs;
    }
    return N > 0 && N <= 5 && total_us < kGestureMaxUs;
}
static_assert(IsValidGesture(kHappyGesture) && IsValidGesture(kSadGesture) &&
              IsValidGesture(kSurprisedGesture) && IsValidGesture(kAngryGesture) &&
              IsValidGesture(kThinkingGesture) && IsValidGesture(kWinkingGesture));

Emotion ParseEmotion(const char* emotion) {
    if (emotion == nullptr)
        return Emotion::kNone;
    if (strcmp(emotion, "happy") == 0)
        return Emotion::kHappy;
    if (strcmp(emotion, "sad") == 0)
        return Emotion::kSad;
    if (strcmp(emotion, "angry") == 0)
        return Emotion::kAngry;
    if (strcmp(emotion, "surprised") == 0)
        return Emotion::kSurprised;
    if (strcmp(emotion, "thinking") == 0)
        return Emotion::kThinking;
    if (strcmp(emotion, "winking") == 0)
        return Emotion::kWinking;
    return Emotion::kNone;  // Neutral and sleepy remain still.
}

bool IsSleepEmotion(const char* emotion) {
    return emotion != nullptr &&
           (strcmp(emotion, "sleep") == 0 || strcmp(emotion, "sleepy") == 0);
}

const char* MotionName(Motion motion) {
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

const char* AutonomyModeName(AutonomyMode mode) {
    switch (mode) {
        case AutonomyMode::kFollow:
            return "follow";
        case AutonomyMode::kObstacleAvoidance:
            return "obstacle_avoidance";
        default:
            return "manual";
    }
}

const char* DistanceMoveStateName(DistanceMoveState state) {
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
    static L298nController& Instance() {
        static L298nController controller;
        return controller;
    }

    void Stop() {
        std::lock_guard lock(mutex_);
        CancelRouteLocked();
        CancelScanLocked();
        // A spoken stop must not be undone by an emotion in the acknowledgement.
        emotion_enabled_ = false;
        pending_emotion_ = Emotion::kNone;
        manual_override_ = true;
        DisableAutonomyLocked();
        web_session_ = 0;  // A spoken stop also invalidates in-flight browser commands.
        web_sequence_ = 0;
        web_deadline_us_ = 0;
        web_manual_ = false;
        ReleaseWebSleepLockLocked();
        StopLocked();
    }

    esp_err_t SetSpeedPercent(int speed) {
        if (speed < 0 || speed > 100)
            return ESP_ERR_INVALID_ARG;
        std::lock_guard lock(mutex_);
        if (web_manual_)
            return ESP_ERR_INVALID_STATE;
        CancelRouteLocked();
        CancelScanLocked();
        manual_override_ = true;
        pending_emotion_ = Emotion::kNone;
        if (gesture_steps_ != nullptr)
            StopLocked();
        speed_percent_ = speed;
        if (speed == 0)
            DisableAutonomyLocked();
        if (speed == 0 || (motion_ != Motion::kStop && esp_timer_get_time() >= deadline_us_)) {
            StopLocked();
        } else if (moving_ && !ApplySpeedLocked()) {
            StopLocked();
        }
        // Changing speed never extends the deadline or restarts stopped motors.
        // Keep hardware faults distinct from argument/ownership errors; Snapshot retains details.
        return fault_ == ESP_OK ? ESP_OK : ESP_FAIL;
    }

    esp_err_t MoveDistanceFromCommand(bool forward, int distance_cm) {
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
        return StartDistanceMoveCommand(forward ? Motion::kForward : Motion::kBackward,
                                        distance_cm);
#else
        (void)forward;
        (void)distance_cm;
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }

    L298nMotorStatus Snapshot() {
        std::lock_guard lock(mutex_);
        const bool distance_fresh =
            sensor_valid_ &&
            esp_timer_get_time() - last_sensor_update_us_ <= kSensorStaleMs * 1000LL;
        return {MotionName(motion_),
                speed_percent_,
                moving_ ? ActiveSpeedLocked() : 0,
                moving_,
                motion_ != Motion::kStop && !moving_,
                web_manual_,
                emotion_enabled_,
                gesture_steps_ != nullptr,
                AutonomyModeName(autonomy_mode_),
                follow_distance_cm_,
                obstacle_distance_cm_,
                std::min(follow_distance_cm_, kFollowMaxTargetCm),
                ScaleDistanceForSpeed(obstacle_distance_cm_, speed_percent_),
                ScaleDistanceForSpeed(obstacle_distance_cm_, web_speed_percent_),
                distance_fresh ? measured_distance_cm_ : -1,
                distance_fresh,
                avoid_state_ == AvoidState::kBlocked,
                fault_,
                encoder_.Snapshot(),
                DistanceMoveStateName(distance_move_state_),
                distance_move_target_cm_,
                distance_move_progress_cm_,
                kSpeedPiCalibrationValid,
                speed_pi_active_,
                speed_pi_target_rpm_,
                CurrentLeftPwmPercentLocked(),
                CurrentRightPwmPercentLocked(),
                guard_enabled_,
                guard_baseline_cm_,
                ScanStateName(scan_state_),
                scan_distances_[0],
                scan_distances_[1],
                scan_distances_[2],
                ControlOwnerLocked(),
                proximity_warning_enabled_,
                RouteStateName(route_state_),
                route_script_,
                route_active_ ? route_index_ + 1 : 0,
                route_count_};
    }

    void SetWebConnected(bool connected) {
        std::lock_guard lock(mutex_);
        web_connected_ = connected;
        if (!connected)
            CancelScanLocked();
        if (!connected && route_active_ && strcmp(control_owner_, "web") == 0)
            CancelRouteLocked();
        if (!connected && web_manual_) {
            EndWebControlLocked();
        }
    }

    esp_err_t BeginWebControl(uint32_t* session) {
        if (session == nullptr)
            return ESP_ERR_INVALID_ARG;
        std::lock_guard lock(mutex_);
        const int64_t now = esp_timer_get_time();
        if (!web_connected_ || !MotionAllowed())
            return ESP_ERR_INVALID_STATE;
        if (web_session_ != 0 && now < web_deadline_us_)
            return ESP_ERR_INVALID_STATE;
        if (web_manual_)
            EndWebControlLocked();
        if (fault_ != ESP_OK)
            return fault_;
        CancelRouteLocked();
        CancelScanLocked();
        DisableAutonomyLocked();
        StopLocked();
        if (fault_ != ESP_OK)
            return fault_;
#ifdef CONFIG_PM_ENABLE
        if (!Check(esp_pm_lock_acquire(web_sleep_lock_)))
            return ESP_FAIL;
        web_sleep_lock_held_ = true;
#endif
        pending_emotion_ = Emotion::kNone;
        web_manual_ = true;
        proximity_warning_enabled_ = true;
        web_sequence_ = 0;
        // Zero is reserved for an expired/released session.
        web_session_ = esp_random();
        if (web_session_ == 0)
            web_session_ = 1;
        web_deadline_us_ = now + kWebTimeoutMs * 1000LL;
        *session = web_session_;
        return ESP_OK;
    }

    esp_err_t WebCommand(uint32_t session, uint32_t sequence, L298nWebCommand command, int speed,
                         bool proximity_warning_enabled, int joystick_x, int joystick_y) {
        if (speed < 0 || speed > 100 || joystick_x < -1000 || joystick_x > 1000 ||
            joystick_y < -1000 || joystick_y > 1000)
            return ESP_ERR_INVALID_ARG;
        Motion motion;
        switch (command) {
            case L298nWebCommand::kForward:
                motion = Motion::kForward;
                break;
            case L298nWebCommand::kBackward:
                motion = Motion::kBackward;
                break;
            case L298nWebCommand::kLeft:
                motion = Motion::kLeft;
                break;
            case L298nWebCommand::kRight:
                motion = Motion::kRight;
                break;
            case L298nWebCommand::kJoystick:
                motion = joystick_y > 0   ? Motion::kForward
                         : joystick_y < 0 ? Motion::kBackward
                         : joystick_x < 0 ? Motion::kLeft
                         : joystick_x > 0 ? Motion::kRight
                                          : Motion::kStop;
                break;
            case L298nWebCommand::kStop:
            case L298nWebCommand::kRelease:
                motion = Motion::kStop;
                break;
            default:
                return ESP_ERR_INVALID_ARG;
        }
        std::lock_guard lock(mutex_);
        const int64_t now = esp_timer_get_time();
        if (!web_connected_ || session == 0 || session != web_session_ ||
            sequence <= web_sequence_ || now >= web_deadline_us_)
            return ESP_ERR_INVALID_STATE;
        web_sequence_ = sequence;
        proximity_warning_enabled_ = proximity_warning_enabled;
        pending_emotion_ = Emotion::kNone;
        if (command == L298nWebCommand::kRelease) {
            EndWebControlLocked();
            return ESP_OK;
        }
        if (!MotionAllowed() || fault_ != ESP_OK) {
            EndWebControlLocked();
            return ESP_ERR_INVALID_STATE;
        }
        web_deadline_us_ = now + kWebTimeoutMs * 1000LL;
        web_speed_percent_ = speed;
        if (motion == Motion::kStop || speed == 0) {
            StopLocked();
        } else if (command == L298nWebCommand::kJoystick) {
            int left = joystick_y + joystick_x;
            int right = joystick_y - joystick_x;
            const int peak = std::max(std::abs(left), std::abs(right));
            if (peak > 1000) {
                left = left * 1000 / peak;
                right = right * 1000 / peak;
            }
            left = left * speed / 1000;
            right = right * speed / 1000;
            if (left == 0 && right == 0) {
                StopLocked();
            } else {
                if (!web_joystick_active_ && moving_)
                    StopOutputsLocked();
                web_joystick_active_ = true;
                if (!ApplyWebDriveLocked(left, right)) {
                    StopLocked();
                    EndWebControlLocked();
                    return ESP_FAIL;
                }
                motion_ = motion;
                moving_ = true;
                deadline_us_ = web_deadline_us_;
            }
        } else {
            if (web_joystick_active_) {
                StopLocked();
                web_joystick_active_ = false;
            }
            // Heartbeats refresh the deadline without interrupting PWM or the direction delay.
            if (motion_ != motion) {
                StopLocked();
                if (fault_ != ESP_OK || !PrepareMotionLocked(motion, kWebTimeoutMs)) {
                    EndWebControlLocked();
                    return ESP_FAIL;
                }
            }
            deadline_us_ = web_deadline_us_;
            if (moving_ && !ApplySpeedLocked()) {
                EndWebControlLocked();
                return ESP_FAIL;
            }
        }
        return fault_;
    }

    void UpdateEmotion(const char* emotion, bool speaking) {
        const auto next = ParseEmotion(emotion);
        const bool sleep_requested = IsSleepEmotion(emotion);
        std::lock_guard lock(mutex_);
        if (speaking)
            CancelScanLocked();
        if (sleep_requested) {
            response_active_ = speaking;
            last_emotion_ = Emotion::kNone;
            pending_emotion_ = Emotion::kNone;
            manual_override_ = speaking;
            DisableAutonomyLocked();
            // The browser owns manual motion until it releases or times out.
            // Sleep expressions still cancel autonomous and emotion movement.
            if (!web_manual_)
                StopLocked();
            return;
        }
        if (!speaking) {
            if (response_active_) {
                manual_override_ = false;
            }
            response_active_ = false;
            last_emotion_ = Emotion::kNone;
            pending_emotion_ = Emotion::kNone;
            if (gesture_steps_ != nullptr)
                StopLocked();
            return;
        }
        response_active_ = true;
        if (next == last_emotion_)
            return;
        last_emotion_ = next;
        pending_emotion_ = Emotion::kNone;
        // A changed/neutral expression cancels an old gesture, never a manual command.
        if (gesture_steps_ != nullptr)
            StopLocked();
        if (emotion_enabled_ && !manual_override_ && !web_manual_ && !route_active_ &&
            autonomy_mode_ == AutonomyMode::kManual && motion_ == Motion::kStop &&
            speed_percent_ > 0 && fault_ == ESP_OK && esp_timer_get_time() >= next_gesture_at_us_) {
            pending_emotion_ = next;
        }
    }

    esp_err_t SetFollowMode(bool enabled) {
        std::lock_guard lock(mutex_);
        return SetAutonomyModeLocked(enabled ? AutonomyMode::kFollow : AutonomyMode::kManual,
                                     AutonomyMode::kFollow);
    }

    esp_err_t SetObstacleAvoidance(bool enabled) {
        std::lock_guard lock(mutex_);
        return SetAutonomyModeLocked(
            enabled ? AutonomyMode::kObstacleAvoidance : AutonomyMode::kManual,
            AutonomyMode::kObstacleAvoidance);
    }

    esp_err_t SetFollowDistance(int distance_cm) {
        if (distance_cm < kMinFollowDistanceCm || distance_cm > kMaxFollowDistanceCm)
            return ESP_ERR_INVALID_ARG;
        std::lock_guard lock(mutex_);
        follow_distance_cm_ = distance_cm;
        return ESP_OK;
    }

    esp_err_t SetObstacleDistance(int distance_cm) {
        if (distance_cm < kMinObstacleDistanceCm || distance_cm > kMaxObstacleDistanceCm)
            return ESP_ERR_INVALID_ARG;
        std::lock_guard lock(mutex_);
        obstacle_distance_cm_ = distance_cm;
        return ESP_OK;
    }

    esp_err_t SetGuardMode(bool enabled) {
        std::lock_guard lock(mutex_);
        if (enabled && (web_manual_ || autonomy_mode_ != AutonomyMode::kManual ||
                        motion_ != Motion::kStop || ScanActiveLocked() || route_active_ ||
                        fault_ != ESP_OK ||
                        !MotionAllowed()))
            return ESP_ERR_INVALID_STATE;
        guard_enabled_ = enabled;
        guard_baseline_cm_ = -1;
        guard_change_samples_ = 0;
        guard_alert_pending_ = false;
        return ESP_OK;
    }

    esp_err_t StartScan() {
        std::lock_guard lock(mutex_);
        if (web_manual_ || autonomy_mode_ != AutonomyMode::kManual || motion_ != Motion::kStop ||
            gesture_steps_ != nullptr || brake_active_ || response_active_ || fault_ != ESP_OK ||
            speed_percent_ == 0 || !sensor_valid_ ||
            esp_timer_get_time() - last_sensor_update_us_ > kSensorStaleMs * 1000LL ||
            !MotionAllowed() || ScanActiveLocked() || route_active_)
            return ESP_ERR_INVALID_STATE;
        guard_baseline_cm_ = -1;
        guard_change_samples_ = 0;
        pending_emotion_ = Emotion::kNone;
        manual_override_ = true;
        scan_distances_[0] = scan_distances_[1] = scan_distances_[2] = -1;
        scan_state_ = ScanState::kCenter;
        scan_phase_deadline_us_ = esp_timer_get_time() + kScanSettleUs;
        return ESP_OK;
    }

    void CancelScan() {
        std::lock_guard lock(mutex_);
        CancelScanLocked();
    }

    esp_err_t SaveRoute(const char* script) {
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

    esp_err_t RunRoute(const char* owner) {
#ifndef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
        (void)owner;
        return ESP_ERR_NOT_SUPPORTED;
#else
        std::lock_guard lock(mutex_);
        const auto encoder = encoder_.Snapshot();
        if (route_count_ == 0 || route_active_ || web_manual_ ||
            (owner != nullptr && strcmp(owner, "web") == 0 && !web_connected_) ||
            autonomy_mode_ != AutonomyMode::kManual || ScanActiveLocked() ||
            motion_ != Motion::kStop || brake_active_ || response_active_ ||
            !encoder.ready || !encoder.calibrated || fault_ != ESP_OK ||
            speed_percent_ == 0 || !MotionAllowed())
            return ESP_ERR_INVALID_STATE;
        CancelScanLocked();
        pending_emotion_ = Emotion::kNone;
        manual_override_ = true;
        route_active_ = true;
        route_state_ = RouteState::kRunning;
        route_index_ = 0;
        route_deadline_us_ = esp_timer_get_time() + kRouteMaxUs;
        control_owner_ = owner != nullptr && strcmp(owner, "telegram") == 0 ? "telegram" :
                         owner != nullptr && strcmp(owner, "web") == 0 ? "web" : "voice";
        if (!StartRouteStepLocked()) {
            CancelRouteLocked();
            return ESP_FAIL;
        }
        return ESP_OK;
#endif
    }

    void CancelRoute() {
        std::lock_guard lock(mutex_);
        CancelRouteLocked();
    }

    void UpdateDistance(int distance_cm, bool valid) {
        std::lock_guard lock(mutex_);
        const int64_t now = esp_timer_get_time();
        last_sensor_update_us_ = now;
        sensor_valid_ = valid;
        measured_distance_cm_ = valid ? distance_cm : -1;
        if (scan_state_ == ScanState::kCenter || scan_state_ == ScanState::kLeft ||
            scan_state_ == ScanState::kRight) {
            if (now >= scan_phase_deadline_us_) {
                const int slot = scan_state_ == ScanState::kLeft ? 0 :
                                 scan_state_ == ScanState::kCenter ? 1 : 2;
                scan_distances_[slot] = valid ? distance_cm : -1;
                const ScanState next = scan_state_ == ScanState::kCenter ? ScanState::kTurnLeft :
                                       scan_state_ == ScanState::kLeft ? ScanState::kTurnRight :
                                       ScanState::kReturn;
                const Motion turn = next == ScanState::kTurnRight ? Motion::kRight : Motion::kLeft;
                scan_state_ = next;
                autonomy_speed_percent_ = std::min(speed_percent_, kAutonomyTurnSpeedPercent);
                if (!PrepareMotionLocked(turn, next == ScanState::kTurnRight ? 2 * kScanTurnMs
                                                                         : kScanTurnMs))
                    CancelScanLocked();
            }
            return;
        }
        if (guard_enabled_ && !ScanActiveLocked() && !route_active_ &&
            autonomy_mode_ == AutonomyMode::kManual &&
            !web_manual_ && motion_ == Motion::kStop && !response_active_ && MotionAllowed()) {
            if (!valid) {
                guard_change_samples_ = 0;
            } else if (guard_baseline_cm_ < 0) {
                guard_baseline_cm_ = distance_cm;
            } else if (std::abs(distance_cm - guard_baseline_cm_) >= kGuardChangeCm) {
                if (++guard_change_samples_ >= 3 && now >= guard_next_alert_us_) {
                    guard_alert_distance_cm_ = distance_cm;
                    guard_alert_pending_ = true;
                    guard_next_alert_us_ = now + kGuardCooldownUs;
                    guard_baseline_cm_ = distance_cm;
                    guard_change_samples_ = 0;
                }
            } else {
                guard_change_samples_ = 0;
                guard_baseline_cm_ = (3 * guard_baseline_cm_ + distance_cm) / 4;
            }
        } else {
            guard_baseline_cm_ = -1;
            guard_change_samples_ = 0;
        }
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
        // One missed echo must not cancel a live scan on the first recovered reading.
        // A phase can only start after two valid samples. Two missed echoes or the
        // existing motor deadline still cancel it; an expired phase is never revived.
        const bool recovering_live_scan =
            autonomy_mode_ == AutonomyMode::kObstacleAvoidance && AvoidMotionPhaseLocked() &&
            motion_ != Motion::kStop && esp_timer_get_time() < deadline_us_;
        if (valid_sensor_samples_ < 2 && !recovering_live_scan) {
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

    void ScheduleProximityWarning() {
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
                !warning_queued_ &&
                now >= next_warning_at_us_) {
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

    void ScheduleGuardAlert() {
        int distance = -1;
        {
            std::lock_guard lock(mutex_);
            if (!guard_alert_pending_)
                return;
            guard_alert_pending_ = false;
            distance = guard_alert_distance_cm_;
        }
        Application::GetInstance().Schedule([distance]() {
            auto& app = Application::GetInstance();
#if defined(CONFIG_BREAD_COMPACT_WIFI_LCD_ST7735_80X160) || defined(CONFIG_LCD_ST7789_240X240)
            if (auto* display = Board::GetInstance().GetDisplay())
                static_cast<RobotEyeDisplay*>(display)->ShowAlertFor(3500);
#endif
            if (app.CanEnterSleepMode())
                app.PlaySound(Lang::Sounds::OGG_POPUP);
        });
        char message[160];
        snprintf(message, sizeof(message),
                 "⚠️ Ruby: khoảng cách trước xe thay đổi bất thường (%d cm). Chế độ canh gác đang bật.",
                 distance);
        QueueL298nTelegramAlert(message);
    }

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
    int duration_ms_ = kDefaultDurationMs;
    int64_t start_at_us_ = 0;
    int64_t deadline_us_ = 0;
    bool moving_ = false;
    esp_err_t fault_ = ESP_OK;
    bool emotion_enabled_ = true;
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
    Emotion last_emotion_ = Emotion::kNone;
    Emotion pending_emotion_ = Emotion::kNone;  // One mailbox; no motion queue.
    const GestureStep* gesture_steps_ = nullptr;
    size_t gesture_count_ = 0;
    size_t gesture_index_ = 0;
    int64_t gesture_deadline_us_ = 0;
    int64_t next_gesture_at_us_ = 0;
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
    int avoid_phase_duration_ms_ = 0;
    int avoid_first_distance_cm_ = -1;
    int avoid_sample_distance_cm_ = -1;
    bool sensor_valid_ = false;
    bool warning_queued_ = false;
    bool proximity_warning_enabled_ = true;
    int64_t next_warning_at_us_ = 0;
    bool guard_enabled_ = false;
    int guard_baseline_cm_ = -1;
    int guard_change_samples_ = 0;
    int guard_alert_distance_cm_ = -1;
    bool guard_alert_pending_ = false;
    int64_t guard_next_alert_us_ = 0;
    ScanState scan_state_ = ScanState::kIdle;
    int scan_distances_[3] = {-1, -1, -1};
    int64_t scan_phase_deadline_us_ = 0;
    std::array<RouteStep, kRouteMaxSteps> route_steps_ = {};
    std::array<char, 64> route_script_ = {};
    int route_count_ = 0;
    int route_index_ = 0;
    int64_t route_deadline_us_ = 0;
    bool route_active_ = false;
    RouteState route_state_ = RouteState::kEmpty;
    const char* control_owner_ = "none";

    const char* ControlOwnerLocked() const {
        if (web_manual_)
            return "web";
        if (route_active_)
            return control_owner_;
        if (autonomy_mode_ != AutonomyMode::kManual || ScanActiveLocked())
            return "automatic";
        return motion_ != Motion::kStop ? control_owner_ : "none";
    }

    void CancelRouteLocked() {
        if (!route_active_)
            return;
        route_active_ = false;
        route_state_ = RouteState::kInterrupted;
        StopLocked();
        manual_override_ = response_active_;
    }

    bool StartRouteStepLocked() {
        if (route_index_ >= route_count_)
            return false;
        const auto& step = route_steps_[route_index_];
        if (step.motion == Motion::kForward &&
            (!sensor_valid_ || esp_timer_get_time() - last_sensor_update_us_ >
                                   kSensorStaleMs * 1000LL ||
             measured_distance_cm_ <= ScaleDistanceForSpeed(obstacle_distance_cm_, speed_percent_)))
            return false;
        StopLocked();
        if (!PrepareMotionLocked(step.motion, DistanceMoveTimeoutMs(step.distance_cm)))
            return false;
        distance_move_target_cm_ = step.distance_cm;
        distance_move_progress_cm_ = 0;
        distance_profile_speed_percent_ = 0;
        distance_profile_update_us_ = 0;
        distance_move_state_ = DistanceMoveState::kPending;
        return true;
    }

    bool ScanActiveLocked() const {
        return scan_state_ == ScanState::kCenter || scan_state_ == ScanState::kTurnLeft ||
               scan_state_ == ScanState::kLeft || scan_state_ == ScanState::kTurnRight ||
               scan_state_ == ScanState::kRight || scan_state_ == ScanState::kReturn;
    }

    void CancelScanLocked() {
        if (!ScanActiveLocked())
            return;
        scan_state_ = ScanState::kInterrupted;
        StopLocked();
        manual_override_ = response_active_;
    }

    L298nController() {
        constexpr uint64_t pins = (1ULL << MOTOR_LEFT_EN_PIN) | (1ULL << MOTOR_LEFT_IN1_PIN) |
                                  (1ULL << MOTOR_LEFT_IN2_PIN) | (1ULL << MOTOR_RIGHT_EN_PIN) |
                                  (1ULL << MOTOR_RIGHT_IN1_PIN) | (1ULL << MOTOR_RIGHT_IN2_PIN);
        gpio_config_t gpio = {};
        gpio.pin_bit_mask = pins;
        gpio.mode = GPIO_MODE_OUTPUT;
        gpio.pull_down_en = GPIO_PULLDOWN_ENABLE;
        ESP_ERROR_CHECK(gpio_config(&gpio));
        for (auto pin : {MOTOR_LEFT_EN_PIN, MOTOR_LEFT_IN1_PIN, MOTOR_LEFT_IN2_PIN,
                         MOTOR_RIGHT_EN_PIN, MOTOR_RIGHT_IN1_PIN, MOTOR_RIGHT_IN2_PIN}) {
            ESP_ERROR_CHECK(gpio_set_level(pin, 0));
        }

        ledc_timer_config_t pwm = {};
        pwm.speed_mode = kMode;
        pwm.duty_resolution = LEDC_TIMER_10_BIT;
        pwm.timer_num = kTimer;
        pwm.freq_hz = 1000;
        pwm.clk_cfg = LEDC_AUTO_CLK;
        ESP_ERROR_CHECK(ledc_timer_config(&pwm));
        for (auto channel : {kLeftChannel, kRightChannel}) {
            ledc_channel_config_t output = {};
            output.gpio_num = channel == kLeftChannel ? MOTOR_LEFT_EN_PIN : MOTOR_RIGHT_EN_PIN;
            output.speed_mode = kMode;
            output.channel = channel;
            output.timer_sel = kTimer;
            output.duty = 0;
            ESP_ERROR_CHECK(ledc_channel_config(&output));
        }
#ifdef CONFIG_PM_ENABLE
        ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "l298n", &sleep_lock_));
        ESP_ERROR_CHECK(
            esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "l298n_web", &web_sleep_lock_));
#endif
        encoder_.Initialize();
        esp_timer_create_args_t timer = {};
        timer.callback = [](void* arg) { static_cast<L298nController*>(arg)->Tick(); };
        timer.arg = this;
        timer.dispatch_method = ESP_TIMER_TASK;
        timer.name = "l298n_deadline";
        timer.skip_unhandled_events = true;
        ESP_ERROR_CHECK(esp_timer_create(&timer, &timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_, kTickUs));
        RegisterTools();
    }

    ~L298nController() {
        esp_timer_stop(timer_);
        esp_timer_delete(timer_);
        Stop();
#ifdef CONFIG_PM_ENABLE
        esp_pm_lock_delete(sleep_lock_);
        esp_pm_lock_delete(web_sleep_lock_);
#endif
    }

    bool Check(esp_err_t error) {
        if (error != ESP_OK && fault_ == ESP_OK) {
            fault_ = error;  // Latch hardware errors until reboot; never auto-resume.
        }
        return error == ESP_OK;
    }

    void DisableAutonomyLocked() {
        autonomy_mode_ = AutonomyMode::kManual;
        avoid_state_ = AvoidState::kForward;
        avoid_clear_samples_ = 0;
        avoid_phase_deadline_us_ = 0;
        avoid_phase_duration_ms_ = 0;
        avoid_first_distance_cm_ = -1;
        avoid_sample_distance_cm_ = -1;
        valid_sensor_samples_ = 0;
        invalid_sensor_samples_ = 0;
    }

    esp_err_t SetAutonomyModeLocked(AutonomyMode requested, AutonomyMode controlled_mode) {
        if (requested == AutonomyMode::kManual && autonomy_mode_ != controlled_mode)
            return ESP_OK;
        if (web_manual_)
            return ESP_ERR_INVALID_STATE;
        CancelRouteLocked();
        CancelScanLocked();
        if (requested != AutonomyMode::kManual &&
            (!MotionAllowed() || fault_ != ESP_OK || speed_percent_ == 0)) {
            return ESP_ERR_INVALID_STATE;
        }
        StopLocked();
        pending_emotion_ = Emotion::kNone;
        manual_override_ = requested != AutonomyMode::kManual;
        autonomy_mode_ = requested;
        avoid_state_ = AvoidState::kForward;
        avoid_clear_samples_ = 0;
        avoid_phase_deadline_us_ = 0;
        avoid_phase_duration_ms_ = 0;
        avoid_first_distance_cm_ = -1;
        avoid_sample_distance_cm_ = -1;
        valid_sensor_samples_ = 0;
        invalid_sensor_samples_ = 0;
        return ESP_OK;
    }

    bool RefreshAutonomyMotionLocked(Motion motion, int speed_percent) {
        const int64_t now = esp_timer_get_time();
        autonomy_speed_percent_ = std::min(speed_percent_, speed_percent);
        if (autonomy_speed_percent_ <= 0) {
            StopLocked();
            return true;
        }
        if (motion_ != motion) {
            StopLocked();
            return fault_ == ESP_OK && PrepareMotionLocked(motion, kAutonomyCommandMs);
        }
        deadline_us_ = now + kAutonomyCommandMs * 1000LL;
        duration_ms_ = kAutonomyCommandMs;
        if (moving_ && !ApplySpeedLocked()) {
            StopLocked();
            return false;
        }
        return true;
    }

    void UpdateFollowLocked(int distance_cm) {
        const int target_distance = std::min(follow_distance_cm_, kFollowMaxTargetCm);
        avoid_state_ = AvoidState::kForward;
        if (distance_cm > kFollowMaxTargetCm) {
            StopLocked();
            return;
        }
        // Start retreating only at close range, then keep reversing until the effective
        // target is reached. A stop (including sensor loss) cancels the retreat.
        if (distance_cm < target_distance &&
            (distance_cm <= kFollowReverseTriggerCm || motion_ == Motion::kBackward)) {
            RefreshAutonomyMotionLocked(Motion::kBackward, kFollowSpeedPercent);
            return;
        }
        if (distance_cm <= target_distance + kFollowHysteresisCm) {
            StopLocked();
            return;
        }
        RefreshAutonomyMotionLocked(Motion::kForward, kFollowSpeedPercent);
    }

    bool AvoidMotionPhaseLocked() const {
        return avoid_state_ == AvoidState::kReverse || avoid_state_ == AvoidState::kTurnFirst ||
               avoid_state_ == AvoidState::kTurnSecond || avoid_state_ == AvoidState::kReturnFirst;
    }

    void BlockAvoidLocked() {
        StopLocked();
        avoid_state_ = AvoidState::kBlocked;
        avoid_clear_samples_ = 0;
        avoid_phase_deadline_us_ = 0;
        avoid_phase_duration_ms_ = 0;
        avoid_first_distance_cm_ = -1;
        avoid_sample_distance_cm_ = -1;
    }

    void StartAvoidMotionLocked(AvoidState state, Motion motion, int duration_ms) {
        StopLocked();
        avoid_state_ = state;
        avoid_clear_samples_ = 0;
        avoid_phase_duration_ms_ = duration_ms;
        // Tick starts this clock when the bridge is actually enabled, after direction delay.
        avoid_phase_deadline_us_ = 0;
        const int speed = state == AvoidState::kReverse ? kAvoidReverseSpeedPercent
                                                       : kAutonomyTurnSpeedPercent;
        if (!RefreshAutonomyMotionLocked(motion, speed))
            BlockAvoidLocked();
    }

    void FinishAvoidMotionLocked(int64_t now) {
        StopLocked();
        if (avoid_state_ == AvoidState::kReverse) {
            StartAvoidMotionLocked(AvoidState::kTurnFirst, Motion::kLeft, kAvoidSideTurnMs);
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

    void DriveAvoidForwardLocked() {
        avoid_state_ = AvoidState::kForward;
        avoid_phase_deadline_us_ = 0;
        avoid_clear_samples_ = 0;
        if (!RefreshAutonomyMotionLocked(Motion::kForward, kAutonomySpeedPercent))
            BlockAvoidLocked();
    }

    void UpdateObstacleAvoidanceLocked(int distance_cm) {
        const int forward_speed = speed_percent_;
        const int stop_distance = ScaleDistanceForSpeed(obstacle_distance_cm_, forward_speed);
        const int clear_distance = stop_distance + kAvoidClearanceCm;
        const int64_t now = esp_timer_get_time();
        if (avoid_state_ == AvoidState::kBlocked) {
            if (distance_cm > clear_distance) {
                if (++avoid_clear_samples_ >= 3) {
                    DriveAvoidForwardLocked();
                }
            } else {
                avoid_clear_samples_ = 0;
                StopLocked();
            }
            return;
        }
        if (avoid_state_ == AvoidState::kForward) {
            if (distance_cm <= stop_distance) {
                avoid_first_distance_cm_ = -1;
                StartAvoidMotionLocked(AvoidState::kReverse, Motion::kBackward, kAvoidReverseMs);
            } else {
                DriveAvoidForwardLocked();
            }
            return;
        }

        if (AvoidMotionPhaseLocked()) {
            // An interrupted scan no longer has a trustworthy timed heading.
            if (motion_ == Motion::kStop || (moving_ && now >= deadline_us_)) {
                BlockAvoidLocked();
                return;
            }
            if (avoid_phase_deadline_us_ != 0 && now >= avoid_phase_deadline_us_) {
                FinishAvoidMotionLocked(now);
                return;
            }
            Motion motion = Motion::kLeft;
            if (avoid_state_ == AvoidState::kReverse)
                motion = Motion::kBackward;
            else if (avoid_state_ == AvoidState::kTurnSecond)
                motion = Motion::kRight;
            const int speed = avoid_state_ == AvoidState::kReverse ? kAvoidReverseSpeedPercent
                                                                 : kAutonomyTurnSpeedPercent;
            if (!RefreshAutonomyMotionLocked(motion, speed))
                BlockAvoidLocked();
            return;
        }

        // Measure only while stationary, after allowing old-heading samples to leave the filter.
        if (now < avoid_phase_deadline_us_)
            return;
        avoid_sample_distance_cm_ = avoid_clear_samples_ == 0
                                        ? distance_cm
                                        : std::min(avoid_sample_distance_cm_, distance_cm);
        if (++avoid_clear_samples_ < 2)
            return;
        const int measured = avoid_sample_distance_cm_;
        if (avoid_state_ == AvoidState::kMeasureFirst) {
            avoid_first_distance_cm_ = measured;
            if (measured > std::max(kAvoidFirstClearCm, stop_distance)) {
                DriveAvoidForwardLocked();
                return;
            }
            // From the left probe to the right probe: cross twice the first turn angle.
            StartAvoidMotionLocked(AvoidState::kTurnSecond, Motion::kRight, 2 * kAvoidSideTurnMs);
            return;
        }
        if (avoid_state_ == AvoidState::kMeasureSecond) {
            if (std::max(avoid_first_distance_cm_, measured) <= stop_distance) {
                BlockAvoidLocked();
            } else if (measured >= avoid_first_distance_cm_) {
                DriveAvoidForwardLocked();
            } else {
                StartAvoidMotionLocked(AvoidState::kReturnFirst, Motion::kLeft,
                                       2 * kAvoidSideTurnMs);
            }
            return;
        }
        // Re-check the chosen left heading after returning; the obstacle may have moved.
        if (measured > stop_distance)
            DriveAvoidForwardLocked();
        else
            BlockAvoidLocked();
    }

    void StopLocked() {
        if (distance_move_state_ == DistanceMoveState::kPending ||
            distance_move_state_ == DistanceMoveState::kMoving)
            distance_move_state_ = DistanceMoveState::kInterrupted;
        gesture_steps_ = nullptr;
        gesture_count_ = 0;
        gesture_index_ = 0;
        gesture_deadline_us_ = 0;
        web_joystick_active_ = false;
        StopOutputsLocked();
    }

    void UpdateDistanceMoveLocked() {
        if (distance_move_state_ != DistanceMoveState::kPending &&
            distance_move_state_ != DistanceMoveState::kMoving)
            return;
        const auto encoder = encoder_.Snapshot();
        if (!encoder.ready || !encoder.calibrated) {
            distance_move_state_ = DistanceMoveState::kEncoderError;
            StopLocked();
            return;
        }
        if (distance_move_state_ != DistanceMoveState::kMoving)
            return;
        if (route_active_ && motion_ == Motion::kForward &&
            (!sensor_valid_ || esp_timer_get_time() - last_sensor_update_us_ >
                                   kSensorStaleMs * 1000LL)) {
            distance_move_state_ = DistanceMoveState::kInterrupted;
            StopLocked();
            return;
        }
        if (motion_ == Motion::kForward && sensor_valid_ &&
            esp_timer_get_time() - last_sensor_update_us_ <= kSensorStaleMs * 1000LL &&
            measured_distance_cm_ <= ScaleDistanceForSpeed(obstacle_distance_cm_, speed_percent_)) {
            distance_move_state_ = DistanceMoveState::kObstacle;
            StopLocked();
            return;
        }
        const double left_cm =
            std::max(0.0, (encoder.left.distance_m - distance_move_start_left_m_) * 100.0);
        const double right_cm =
            std::max(0.0, (encoder.right.distance_m - distance_move_start_right_m_) * 100.0);
        distance_move_progress_cm_ = std::min(left_cm, right_cm);
        if (distance_move_progress_cm_ >= distance_move_target_cm_) {
            distance_move_state_ = DistanceMoveState::kCompleted;
            gesture_steps_ = nullptr;
            gesture_count_ = 0;
            gesture_index_ = 0;
            gesture_deadline_us_ = 0;
            web_joystick_active_ = false;
            BeginDistanceBrakeLocked(esp_timer_get_time());
        } else if (encoder.left.no_pulses || encoder.right.no_pulses) {
            distance_move_state_ = DistanceMoveState::kNoPulses;
            StopLocked();
        }
    }

    void UpdateDistanceSpeedProfileLocked(int64_t now) {
        if (distance_move_state_ != DistanceMoveState::kMoving || !moving_ ||
            now - distance_profile_update_us_ < kDistanceRampUpdateUs)
            return;

        const int target_speed = route_active_ ?
                                     std::min(std::max(speed_percent_, kDistanceMinimumDrivePercent),
                                              kAutonomyTurnSpeedPercent) :
                                     std::max(speed_percent_, kDistanceMinimumDrivePercent);
        const int start_speed = std::min(target_speed, kDistanceRampStartSpeedPercent);
        const int crawl_speed = std::min(target_speed, kDistanceCrawlSpeedPercent);
        const double total_distance = distance_move_target_cm_;
        const double progress = std::clamp(distance_move_progress_cm_, 0.0, total_distance);
        const double remaining = total_distance - progress;
        // Short moves get a triangular ramp; longer moves reach the requested speed
        // and begin slowing down over the final 8 cm.
        const double peak_fraction = std::min(1.0, total_distance / (2.0 * kDistanceRampLengthCm));
        const double acceleration_fraction = std::min(1.0, progress / kDistanceRampLengthCm);
        const double deceleration_fraction = std::min(1.0, remaining / kDistanceRampLengthCm);
        const double acceleration_limit = std::min(peak_fraction, acceleration_fraction);
        const double deceleration_limit = std::min(peak_fraction, deceleration_fraction);
        const int accelerating_speed =
            start_speed + static_cast<int>(std::lround((target_speed - start_speed) *
                                                       acceleration_limit));
        const int decelerating_speed =
            crawl_speed + static_cast<int>(std::lround((target_speed - crawl_speed) *
                                                       deceleration_limit));
        const int next_speed = std::min(accelerating_speed, decelerating_speed);
        distance_profile_update_us_ = now;
        if (next_speed != distance_profile_speed_percent_) {
            distance_profile_speed_percent_ = next_speed;
            if (!ApplySpeedLocked())
                StopLocked();
        }
    }

    void ReleaseWebSleepLockLocked() {
#ifdef CONFIG_PM_ENABLE
        if (web_sleep_lock_held_) {
            Check(esp_pm_lock_release(web_sleep_lock_));
            web_sleep_lock_held_ = false;
        }
#endif
    }

    void EndWebControlLocked() {
        web_session_ = 0;
        web_sequence_ = 0;
        web_deadline_us_ = 0;
        web_manual_ = false;
        pending_emotion_ = Emotion::kNone;
        ReleaseWebSleepLockLocked();
        // Do not replay a gesture from the response that was active while the browser owned it.
        manual_override_ = response_active_;
        StopLocked();
    }

    void StopOutputsLocked() {
        brake_active_ = false;
        ResetSpeedPiLocked();
        encoder_.SetDrive(0, 0, esp_timer_get_time());
        // Disable both bridges before touching any direction pin. Attempt every
        // output even if an earlier operation fails. Stop means coast, not brake.
        Check(ledc_stop(kMode, kLeftChannel, 0));
        Check(ledc_stop(kMode, kRightChannel, 0));
        for (auto pin :
             {MOTOR_LEFT_IN1_PIN, MOTOR_LEFT_IN2_PIN, MOTOR_RIGHT_IN1_PIN, MOTOR_RIGHT_IN2_PIN}) {
            Check(gpio_set_level(pin, 0));
        }
        motion_ = Motion::kStop;
        moving_ = false;
        web_left_output_percent_ = 0;
        web_right_output_percent_ = 0;
        start_at_us_ = 0;
        deadline_us_ = 0;
#ifdef CONFIG_PM_ENABLE
        if (sleep_lock_held_) {
            Check(esp_pm_lock_release(sleep_lock_));
            sleep_lock_held_ = false;
        }
#endif
    }

    void BeginDistanceBrakeLocked(int64_t now) {
        ResetSpeedPiLocked();
        encoder_.SetDrive(0, 0, now);
        bool ok = true;
        // L298N brakes dynamically when EN is high and both direction inputs match.
        ok = Check(gpio_set_level(MOTOR_LEFT_IN1_PIN, 0)) && ok;
        ok = Check(gpio_set_level(MOTOR_LEFT_IN2_PIN, 0)) && ok;
        ok = Check(gpio_set_level(MOTOR_RIGHT_IN1_PIN, 0)) && ok;
        ok = Check(gpio_set_level(MOTOR_RIGHT_IN2_PIN, 0)) && ok;
        for (auto channel : {kLeftChannel, kRightChannel}) {
            ok = Check(ledc_set_duty(kMode, channel, 1023)) && ok;
            ok = Check(ledc_update_duty(kMode, channel)) && ok;
        }
        if (!ok) {
            StopOutputsLocked();
            return;
        }
        brake_active_ = true;
        brake_until_us_ = now + kDistanceBrakeUs;
        motion_ = Motion::kStop;
        moving_ = false;
        web_left_output_percent_ = 0;
        web_right_output_percent_ = 0;
        start_at_us_ = 0;
        deadline_us_ = 0;
    }

    void ResetSpeedPiLocked() {
        speed_pi_integral_[0] = 0;
        speed_pi_integral_[1] = 0;
        speed_pi_output_percent_[0] = 0;
        speed_pi_output_percent_[1] = 0;
        speed_pi_target_rpm_ = 0;
        speed_pi_startup_until_us_ = 0;
        speed_pi_last_update_us_ = 0;
        speed_pi_active_ = false;
    }

    bool ApplyMotorPwmLocked(int left_percent, int right_percent) {
        const uint32_t left_duty = static_cast<uint32_t>(left_percent) * 1023 / 100;
        const uint32_t right_duty = static_cast<uint32_t>(right_percent) * 1023 / 100;
        return Check(ledc_set_duty(kMode, kLeftChannel, left_duty)) &&
               Check(ledc_update_duty(kMode, kLeftChannel)) &&
               Check(ledc_set_duty(kMode, kRightChannel, right_duty)) &&
               Check(ledc_update_duty(kMode, kRightChannel));
    }

    bool ApplySpeedLocked() {
        const int base_speed = ActiveSpeedLocked();
        if (speed_pi_active_)
            return ApplyMotorPwmLocked(speed_pi_output_percent_[0], speed_pi_output_percent_[1]);
        if (kSpeedPiCalibrationValid && moving_ && speed_pi_startup_until_us_ > 0 &&
            esp_timer_get_time() < speed_pi_startup_until_us_) {
            const int kick_speed = std::max(base_speed, kSpeedPiStartupPwmPercent);
            return ApplyMotorPwmLocked(kick_speed, kick_speed);
        }
        return ApplyMotorPwmLocked(base_speed, base_speed);
    }

    int CurrentPwmPercentLocked(int side) const {
        if (!moving_)
            return 0;
        if (web_manual_)
            return std::abs(side == 0 ? web_left_output_percent_ : web_right_output_percent_);
        if (speed_pi_active_)
            return speed_pi_output_percent_[side];
        const int base_speed = ActiveSpeedLocked();
        if (kSpeedPiCalibrationValid && speed_pi_startup_until_us_ > esp_timer_get_time())
            return std::max(base_speed, kSpeedPiStartupPwmPercent);
        return base_speed;
    }

    int CurrentLeftPwmPercentLocked() const {
        return CurrentPwmPercentLocked(0);
    }

    int CurrentRightPwmPercentLocked() const {
        return CurrentPwmPercentLocked(1);
    }

    void UpdateSpeedPiLocked(int64_t now) {
        if (!kSpeedPiCalibrationValid || !moving_ || web_manual_ || ActiveSpeedLocked() <= 0) {
            if (!moving_ || web_manual_)
                ResetSpeedPiLocked();
            return;
        }
        if (now < speed_pi_startup_until_us_)
            return;
        if (speed_pi_last_update_us_ != 0 && now - speed_pi_last_update_us_ < kSpeedPiUpdateUs)
            return;

        const auto encoder = encoder_.Snapshot();
        if (!encoder.ready || !encoder.calibrated) {
            ResetSpeedPiLocked();
            const int fallback = ActiveSpeedLocked();
            if (!ApplyMotorPwmLocked(fallback, fallback))
                StopLocked();
            return;
        }

        const double requested_pwm =
            std::max(ActiveSpeedLocked(), kSpeedPiMinimumPwmPercent);
        const double left_curve_rpm =
            CalibratedRpmAtPwm(requested_pwm, kLeftRpmAtPwm50, kLeftRpmAtPwm70);
        const double right_curve_rpm =
            CalibratedRpmAtPwm(requested_pwm, kRightRpmAtPwm50, kRightRpmAtPwm70);
        const double target_rpm = (left_curve_rpm + right_curve_rpm) * 0.5;
        const double feed_forward[] = {
            CalibratedPwmForRpm(target_rpm, kLeftRpmAtPwm50, kLeftRpmAtPwm70),
            CalibratedPwmForRpm(target_rpm, kRightRpmAtPwm50, kRightRpmAtPwm70)};
        const double measured_rpm[] = {encoder.left.rpm, encoder.right.rpm};
        int outputs[2] = {};
        const double dt = speed_pi_last_update_us_ == 0
                              ? kSpeedPiUpdateUs / 1000000.0
                              : std::min(0.25, (now - speed_pi_last_update_us_) / 1000000.0);

        for (int i = 0; i < 2; ++i) {
            if ((i == 0 && encoder.left.no_pulses) || (i == 1 && encoder.right.no_pulses)) {
                outputs[i] = std::clamp(static_cast<int>(std::lround(feed_forward[i])),
                                        kSpeedPiMinimumPwmPercent, 100);
                speed_pi_integral_[i] = 0;
                continue;
            }
            const double error = target_rpm - measured_rpm[i];
            const double candidate_integral =
                std::clamp(speed_pi_integral_[i] + error * dt, -kSpeedPiIntegralLimit,
                           kSpeedPiIntegralLimit);
            const double raw_output =
                feed_forward[i] + kSpeedPiKp * error + kSpeedPiKi * candidate_integral;
            const double limited_output =
                std::clamp(raw_output, static_cast<double>(kSpeedPiMinimumPwmPercent), 100.0);
            if (raw_output == limited_output ||
                (raw_output > 100.0 && error < 0) ||
                (raw_output < kSpeedPiMinimumPwmPercent && error > 0)) {
                speed_pi_integral_[i] = candidate_integral;
            }
            outputs[i] = static_cast<int>(std::lround(limited_output));
        }

        speed_pi_target_rpm_ = target_rpm;
        speed_pi_output_percent_[0] = outputs[0];
        speed_pi_output_percent_[1] = outputs[1];
        speed_pi_last_update_us_ = now;
        speed_pi_active_ = true;
        if (!ApplyMotorPwmLocked(outputs[0], outputs[1]))
            StopLocked();
    }

    bool ApplyWebDriveLocked(int left, int right) {
        encoder_.Update(esp_timer_get_time());
        const bool direction_changed = (left != 0 && web_left_output_percent_ != 0 &&
                                        (left < 0) != (web_left_output_percent_ < 0)) ||
                                       (right != 0 && web_right_output_percent_ != 0 &&
                                        (right < 0) != (web_right_output_percent_ < 0));
        if (direction_changed)
            StopOutputsLocked();

        auto set_direction = [this](gpio_num_t in1, gpio_num_t in2, int output, bool inverted) {
            if (output == 0)
                return Check(gpio_set_level(in1, 0)) && Check(gpio_set_level(in2, 0));
            const bool forward = (output > 0) != inverted;
            return Check(gpio_set_level(in1, forward)) && Check(gpio_set_level(in2, !forward));
        };
        auto set_pwm = [this](ledc_channel_t channel, int output) {
            if (output == 0)
                return Check(ledc_stop(kMode, channel, 0));
            const uint32_t duty = static_cast<uint32_t>(std::abs(output)) * 1023 / 100;
            return Check(ledc_set_duty(kMode, channel, duty)) &&
                   Check(ledc_update_duty(kMode, channel));
        };

        bool ok = true;
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_LEFT
        constexpr bool invert_left = true;
#else
        constexpr bool invert_left = false;
#endif
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_RIGHT
        constexpr bool invert_right = true;
#else
        constexpr bool invert_right = false;
#endif
        if (left == 0)
            ok = set_pwm(kLeftChannel, 0) && ok;
        if (right == 0)
            ok = set_pwm(kRightChannel, 0) && ok;
        ok = set_direction(MOTOR_LEFT_IN1_PIN, MOTOR_LEFT_IN2_PIN, left, invert_left) && ok;
        ok = set_direction(MOTOR_RIGHT_IN1_PIN, MOTOR_RIGHT_IN2_PIN, right, invert_right) && ok;
        if (left != 0)
            ok = set_pwm(kLeftChannel, left) && ok;
        if (right != 0)
            ok = set_pwm(kRightChannel, right) && ok;
        if (ok) {
            web_left_output_percent_ = left;
            web_right_output_percent_ = right;
            encoder_.SetDrive(left, right, esp_timer_get_time());
        }
        return ok;
    }

    int ActiveSpeedLocked() const {
        if (distance_move_state_ == DistanceMoveState::kMoving &&
            distance_profile_speed_percent_ > 0)
            return distance_profile_speed_percent_;
        if (web_manual_)
            return web_speed_percent_;
        if (ScanActiveLocked())
            return std::min(speed_percent_, kAutonomyTurnSpeedPercent);
        if (autonomy_mode_ != AutonomyMode::kManual)
            return autonomy_speed_percent_;
        return gesture_steps_ == nullptr
                   ? speed_percent_
                   : std::min(speed_percent_, gesture_steps_[gesture_index_].speed_percent);
    }

    bool PrepareMotionLocked(Motion motion, int duration) {
#ifdef CONFIG_PM_ENABLE
        if (!Check(esp_pm_lock_acquire(sleep_lock_)))
            return false;
        sleep_lock_held_ = true;
#endif
        motion_ = motion;
        duration_ms_ = duration;
        start_at_us_ = esp_timer_get_time() + kDirectionDelayUs;
        deadline_us_ = start_at_us_ + static_cast<int64_t>(duration) * 1000;
        return true;
    }

    void StartGestureStepLocked() {
        StopOutputsLocked();  // Disable PWM before setting up the next direction.
        const auto& step = gesture_steps_[gesture_index_];
        if (fault_ != ESP_OK || !PrepareMotionLocked(step.motion, step.duration_ms)) {
            StopLocked();
        }
    }

    template <size_t N>
    void StartGestureLocked(const GestureStep (&steps)[N], int64_t now) {
        control_owner_ = "voice";
        gesture_steps_ = steps;
        gesture_count_ = N;
        gesture_index_ = 0;
        gesture_deadline_us_ = now + kGestureMaxUs;
        next_gesture_at_us_ = now + kGestureCooldownUs;
        StartGestureStepLocked();
    }

    bool ApplyDirectionLocked() {
        bool left_forward = motion_ == Motion::kForward || motion_ == Motion::kRight;
        bool right_forward = motion_ == Motion::kForward || motion_ == Motion::kLeft;
        // Keep logical wheel signs before applying physical motor wiring inversion.
        const int encoder_left = left_forward ? 1 : -1;
        const int encoder_right = right_forward ? 1 : -1;
        encoder_.Update(esp_timer_get_time());
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_LEFT
        left_forward = !left_forward;
#endif
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_RIGHT
        right_forward = !right_forward;
#endif
        const bool ok = Check(gpio_set_level(MOTOR_LEFT_IN1_PIN, left_forward)) &&
                        Check(gpio_set_level(MOTOR_LEFT_IN2_PIN, !left_forward)) &&
                        Check(gpio_set_level(MOTOR_RIGHT_IN1_PIN, right_forward)) &&
                        Check(gpio_set_level(MOTOR_RIGHT_IN2_PIN, !right_forward));
        if (ok)
            encoder_.SetDrive(encoder_left, encoder_right, esp_timer_get_time());
        ResetSpeedPiLocked();
        return ok;
    }

    static bool MotionAllowed() {
        const auto state = Application::GetInstance().GetDeviceState();
        return state != kDeviceStateUpgrading && state != kDeviceStateFatalError &&
               state != kDeviceStateWifiConfiguring;
    }

    void Tick() {
        std::lock_guard lock(mutex_);
        const int64_t now = esp_timer_get_time();
        // Also sample at rest/while joystick owns the outputs, before any early return.
        encoder_.Update(now);
        if (brake_active_) {
            if (now >= brake_until_us_)
                StopOutputsLocked();
            return;
        }
        UpdateDistanceMoveLocked();
        if (brake_active_)
            return;
        UpdateDistanceSpeedProfileLocked(now);
        UpdateSpeedPiLocked(now);
        if (web_session_ != 0 && (now >= web_deadline_us_ || !MotionAllowed())) {
            EndWebControlLocked();
        }
        const auto state = Application::GetInstance().GetDeviceState();
        if (state == kDeviceStateUpgrading || state == kDeviceStateFatalError ||
            state == kDeviceStateWifiConfiguring) {
            CancelRouteLocked();
            CancelScanLocked();
            pending_emotion_ = Emotion::kNone;
            DisableAutonomyLocked();
            if (motion_ != Motion::kStop)
                StopLocked();
            return;
        }
        if (route_active_) {
            if (fault_ != ESP_OK || now >= route_deadline_us_) {
                CancelRouteLocked();
                return;
            }
            if (distance_move_state_ == DistanceMoveState::kCompleted) {
                if (route_index_ + 1 == route_count_) {
                    route_active_ = false;
                    route_state_ = RouteState::kComplete;
                    manual_override_ = response_active_;
                } else {
                    ++route_index_;
                    if (!StartRouteStepLocked())
                        CancelRouteLocked();
                }
                return;
            }
            if (distance_move_state_ != DistanceMoveState::kPending &&
                distance_move_state_ != DistanceMoveState::kMoving) {
                CancelRouteLocked();
                return;
            }
        }
        if (ScanActiveLocked()) {
            if (fault_ != ESP_OK) {
                CancelScanLocked();
                return;
            }
            if ((scan_state_ == ScanState::kCenter || scan_state_ == ScanState::kLeft ||
                 scan_state_ == ScanState::kRight) &&
                now > scan_phase_deadline_us_ + 1000000) {
                CancelScanLocked();
                return;
            }
            if (scan_state_ == ScanState::kTurnLeft || scan_state_ == ScanState::kTurnRight ||
                scan_state_ == ScanState::kReturn) {
                if (motion_ == Motion::kStop) {
                    CancelScanLocked();
                    return;
                }
                if (now >= deadline_us_) {
                    const ScanState completed = scan_state_;
                    StopLocked();
                    if (completed == ScanState::kReturn) {
                        scan_state_ = ScanState::kComplete;
                        manual_override_ = false;
                    } else {
                        scan_state_ = completed == ScanState::kTurnLeft ? ScanState::kLeft :
                                      ScanState::kRight;
                        scan_phase_deadline_us_ = now + kScanSettleUs;
                    }
                    return;
                }
            } else {
                return;  // The sensor callback advances stationary measurement phases.
            }
        }
        // Web joystick outputs are applied directly from validated commands. Keep the timed
        // direction path active for the four HUD buttons.
        if (web_manual_ && web_joystick_active_)
            return;
        if (autonomy_mode_ != AutonomyMode::kManual &&
            (invalid_sensor_samples_ >= 2 ||
             now - last_sensor_update_us_ > kSensorStaleMs * 1000LL)) {
            avoid_clear_samples_ = 0;
            if (autonomy_mode_ == AutonomyMode::kObstacleAvoidance &&
                avoid_state_ != AvoidState::kForward && avoid_state_ != AvoidState::kBlocked)
                BlockAvoidLocked();
            if (motion_ != Motion::kStop)
                StopLocked();
            return;
        }
        if (autonomy_mode_ == AutonomyMode::kObstacleAvoidance && AvoidMotionPhaseLocked()) {
            if (motion_ == Motion::kStop || now >= deadline_us_) {
                BlockAvoidLocked();
                return;
            }
            if (avoid_phase_deadline_us_ != 0 && now >= avoid_phase_deadline_us_) {
                FinishAvoidMotionLocked(now);
                return;
            }
        }
        if (motion_ == Motion::kStop && pending_emotion_ == Emotion::kNone)
            return;
        if (gesture_steps_ != nullptr && (!response_active_ || state != kDeviceStateSpeaking ||
                                          !emotion_enabled_ || now >= gesture_deadline_us_)) {
            pending_emotion_ = Emotion::kNone;
            StopLocked();
            return;
        }
        if (motion_ == Motion::kStop) {
            const auto emotion = pending_emotion_;
            pending_emotion_ = Emotion::kNone;
            if (!emotion_enabled_ || manual_override_ || web_manual_ || route_active_ ||
                !response_active_ ||
                autonomy_mode_ != AutonomyMode::kManual || state != kDeviceStateSpeaking ||
                speed_percent_ == 0 || fault_ != ESP_OK ||
                now < next_gesture_at_us_)
                return;
            switch (emotion) {
                case Emotion::kHappy:
                    StartGestureLocked(kHappyGesture, now);
                    break;
                case Emotion::kSad:
                    StartGestureLocked(kSadGesture, now);
                    break;
                case Emotion::kAngry:
                    StartGestureLocked(kAngryGesture, now);
                    break;
                case Emotion::kSurprised:
                    StartGestureLocked(kSurprisedGesture, now);
                    break;
                case Emotion::kThinking:
                    StartGestureLocked(kThinkingGesture, now);
                    break;
                case Emotion::kWinking:
                    StartGestureLocked(kWinkingGesture, now);
                    break;
                default:
                    break;
            }
        } else if (now >= deadline_us_) {
            if (gesture_steps_ != nullptr && moving_ && gesture_index_ + 1 < gesture_count_) {
                ++gesture_index_;
                StartGestureStepLocked();
            } else {
                if (distance_move_state_ == DistanceMoveState::kPending ||
                    distance_move_state_ == DistanceMoveState::kMoving)
                    distance_move_state_ = DistanceMoveState::kTimeout;
                StopLocked();
            }
        } else if (!moving_ && now >= start_at_us_) {
            if (route_active_ && motion_ == Motion::kForward &&
                (!sensor_valid_ || now - last_sensor_update_us_ > kSensorStaleMs * 1000LL ||
                 measured_distance_cm_ <=
                     ScaleDistanceForSpeed(obstacle_distance_cm_, speed_percent_))) {
                distance_move_state_ = DistanceMoveState::kInterrupted;
                CancelRouteLocked();
                return;
            }
            if (!ApplyDirectionLocked()) {
                StopLocked();
                return;
            }
            if (distance_move_state_ == DistanceMoveState::kPending) {
                encoder_.Update(esp_timer_get_time());
                const auto encoder = encoder_.Snapshot();
                if (!encoder.ready) {
                    distance_move_state_ = DistanceMoveState::kEncoderError;
                    StopLocked();
                    return;
                }
                distance_move_start_left_m_ = encoder.left.distance_m;
                distance_move_start_right_m_ = encoder.right.distance_m;
                distance_move_state_ = DistanceMoveState::kMoving;
                distance_profile_speed_percent_ =
                    std::max(kDistanceMinimumDrivePercent,
                             std::min(speed_percent_, kDistanceRampStartSpeedPercent));
                distance_profile_update_us_ = esp_timer_get_time();
            }
            moving_ = true;
            ResetSpeedPiLocked();
            if (kSpeedPiCalibrationValid)
                speed_pi_startup_until_us_ = esp_timer_get_time() + kSpeedPiStartupUs;
            if (!ApplySpeedLocked()) {
                StopLocked();
                return;
            }
            // Count the requested duration from enabling the motors. A delayed
            // timer must never start a command that has already expired above.
            // Web deadlines are measured from reception, never from motor start.
            if (!web_manual_)
                deadline_us_ = esp_timer_get_time() + static_cast<int64_t>(duration_ms_) * 1000;
            if (autonomy_mode_ == AutonomyMode::kObstacleAvoidance && AvoidMotionPhaseLocked() &&
                avoid_phase_deadline_us_ == 0) {
                avoid_phase_deadline_us_ = esp_timer_get_time() +
                                          static_cast<int64_t>(avoid_phase_duration_ms_) * 1000;
            }
        }
    }

    ToolResult Move(Motion motion, const PropertyList& properties) {
        const int speed = properties["speed_percent"].value<int>();
        const int duration = properties["duration_ms"].value<int>();
        // Also validate here, independently of MCP's schema validation.
        if (speed < -1 || speed > 100 || duration < 100 || duration > kMaxDurationMs) {
            return std::unexpected("Use speed_percent -1..100 and duration_ms 100..10000.");
        }
        {
            std::lock_guard lock(mutex_);
            if (web_manual_)
                return std::unexpected(
                    "Web manual control is active; release it on the web page first.");
            CancelRouteLocked();
            CancelScanLocked();
            DisableAutonomyLocked();
            manual_override_ = true;
            control_owner_ = "voice";
            pending_emotion_ = Emotion::kNone;
            StopLocked();  // A new movement replaces the previous one, never queues it.
            if (fault_ != ESP_OK) {
                return std::unexpected("Motor hardware error; inspect wiring and restart.");
            }
            const auto state = Application::GetInstance().GetDeviceState();
            if (state == kDeviceStateUpgrading || state == kDeviceStateFatalError ||
                state == kDeviceStateWifiConfiguring) {
                return std::unexpected("Movement unavailable in the current device state.");
            }
            if (speed >= 0) {
                speed_percent_ = speed;
            }
            if (speed_percent_ > 0) {
                if (!PrepareMotionLocked(motion, duration)) {
                    return std::unexpected("Could not keep the motor deadline timer awake.");
                }
            }
        }
        return GetState();
    }

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
    ToolResult MoveDistance(Motion motion, const PropertyList& properties) {
        const int distance_cm = properties["distance_cm"].value<int>();
        const int speed = properties["speed_percent"].value<int>();
        if (distance_cm < 1 || distance_cm > kMaxDistanceMoveCm || speed < -1 || speed > 100) {
            return std::unexpected("Use distance_cm 1..1000 and speed_percent -1..100.");
        }
        {
            std::lock_guard lock(mutex_);
            if (web_manual_)
                return std::unexpected(
                    "Web manual control is active; release it on the web page first.");
            CancelRouteLocked();
            CancelScanLocked();
            const auto encoder = encoder_.Snapshot();
            if (!encoder.ready || !encoder.calibrated) {
                return std::unexpected(
                    "FC-03 is unavailable or uncalibrated; cannot move by distance.");
            }
            if (fault_ != ESP_OK || !MotionAllowed())
                return std::unexpected("Movement unavailable or motor hardware error.");
            const int next_speed = speed >= 0 ? speed : speed_percent_;
            if (next_speed == 0)
                return std::unexpected("Set a nonzero motor speed before moving by distance.");
            DisableAutonomyLocked();
            manual_override_ = true;
            control_owner_ = "voice";
            pending_emotion_ = Emotion::kNone;
            StopLocked();
            speed_percent_ = next_speed;
            if (!PrepareMotionLocked(motion, DistanceMoveTimeoutMs(distance_cm)))
                return std::unexpected("Could not keep the motor deadline timer awake.");
            distance_move_target_cm_ = distance_cm;
            distance_move_progress_cm_ = 0;
            distance_profile_speed_percent_ = 0;
            distance_profile_update_us_ = 0;
            distance_move_state_ = DistanceMoveState::kPending;
        }
        return GetState();
    }

    esp_err_t StartDistanceMoveCommand(Motion motion, int distance_cm) {
        if (distance_cm < 1 || distance_cm > kMaxDistanceMoveCm)
            return ESP_ERR_INVALID_ARG;
        std::lock_guard lock(mutex_);
        if (web_manual_)
            return ESP_ERR_INVALID_STATE;
        CancelRouteLocked();
        CancelScanLocked();
        const auto encoder = encoder_.Snapshot();
        if (!encoder.ready || !encoder.calibrated)
            return ESP_ERR_NOT_SUPPORTED;
        if (fault_ != ESP_OK)
            return fault_;
        if (!MotionAllowed())
            return ESP_ERR_INVALID_STATE;
        if (speed_percent_ == 0)
            return ESP_ERR_INVALID_STATE;
        DisableAutonomyLocked();
        manual_override_ = true;
        control_owner_ = "telegram";
        pending_emotion_ = Emotion::kNone;
        StopLocked();
        if (!PrepareMotionLocked(motion, DistanceMoveTimeoutMs(distance_cm)))
            return ESP_FAIL;
        distance_move_target_cm_ = distance_cm;
        distance_move_progress_cm_ = 0;
        distance_profile_speed_percent_ = 0;
        distance_profile_update_us_ = 0;
        distance_move_state_ = DistanceMoveState::kPending;
        return ESP_OK;
    }
#endif

    ToolResult SetSpeed(int speed) {
        const auto error = SetSpeedPercent(speed);
        if (error == ESP_ERR_INVALID_ARG) {
            return std::unexpected("Use speed_percent 0..100.");
        }
        if (error == ESP_ERR_INVALID_STATE) {
            return std::unexpected(
                "Web manual control is active; release it on the web page first.");
        }
        if (error != ESP_OK) {
            return std::unexpected("Motor hardware error; inspect wiring and restart.");
        }
        return GetState();
    }

    ToolResult SetEmotionEnabled(bool enabled) {
        {
            std::lock_guard lock(mutex_);
            emotion_enabled_ = enabled;
            pending_emotion_ = Emotion::kNone;
            if (gesture_steps_ != nullptr)
                StopLocked();
            // Enabling never starts the last gesture again; wait for a fresh emotion event.
            if (enabled)
                manual_override_ = false;
        }
        return GetState();
    }

    ToolResult GetState() {
        Motion motion;
        AutonomyMode autonomy_mode;
        int speed;
        int active_speed;
        int follow_distance;
        int obstacle_distance;
        int effective_follow_distance;
        int effective_obstacle_distance;
        int measured_distance;
        bool moving;
        bool emotion_enabled;
        bool gesture_active;
        bool web_manual;
        bool distance_valid;
        bool autonomy_blocked;
        int64_t remaining_ms;
        esp_err_t fault;
        bool speed_pi_active;
        bool speed_pi_calibrated;
        double speed_pi_target_rpm;
        int left_pwm_percent;
        int right_pwm_percent;
        Fc03EncoderStatus encoder;
        DistanceMoveState distance_move_state;
        int distance_move_target_cm;
        double distance_move_progress_cm;
        bool guard_enabled;
        int guard_baseline_cm;
        ScanState scan_state;
        int scan_left_cm, scan_center_cm, scan_right_cm;
        const char* control_owner;
        bool warning_sound_enabled;
        RouteState route_state;
        std::array<char, 64> route_script;
        int route_step, route_steps;
        {
            std::lock_guard lock(mutex_);
            const int64_t now = esp_timer_get_time();
            motion = motion_;
            autonomy_mode = autonomy_mode_;
            speed = speed_percent_;
            moving = moving_;
            active_speed = moving_ ? ActiveSpeedLocked() : 0;
            follow_distance = follow_distance_cm_;
            obstacle_distance = obstacle_distance_cm_;
            effective_follow_distance = std::min(follow_distance_cm_, kFollowMaxTargetCm);
            effective_obstacle_distance =
                ScaleDistanceForSpeed(obstacle_distance_cm_, speed_percent_);
            distance_valid =
                sensor_valid_ && now - last_sensor_update_us_ <= kSensorStaleMs * 1000LL;
            measured_distance = distance_valid ? measured_distance_cm_ : -1;
            autonomy_blocked = avoid_state_ == AvoidState::kBlocked;
            emotion_enabled = emotion_enabled_;
            gesture_active = gesture_steps_ != nullptr;
            web_manual = web_manual_;
            fault = fault_;
            speed_pi_calibrated = kSpeedPiCalibrationValid;
            speed_pi_active = speed_pi_active_;
            speed_pi_target_rpm = speed_pi_target_rpm_;
            left_pwm_percent = CurrentLeftPwmPercentLocked();
            right_pwm_percent = CurrentRightPwmPercentLocked();
            encoder = encoder_.Snapshot();
            distance_move_state = distance_move_state_;
            distance_move_target_cm = distance_move_target_cm_;
            distance_move_progress_cm = distance_move_progress_cm_;
            guard_enabled = guard_enabled_;
            guard_baseline_cm = guard_baseline_cm_;
            scan_state = scan_state_;
            scan_left_cm = scan_distances_[0];
            scan_center_cm = scan_distances_[1];
            scan_right_cm = scan_distances_[2];
            control_owner = ControlOwnerLocked();
            warning_sound_enabled = proximity_warning_enabled_;
            route_state = route_state_;
            route_script = route_script_;
            route_step = route_active_ ? route_index_ + 1 : 0;
            route_steps = route_count_;
            remaining_ms = motion == Motion::kStop ? 0
                           : (moving || web_manual)
                               ? std::max<int64_t>(0, (deadline_us_ - now + 999) / 1000)
                               : duration_ms_;
        }
        CJsonUniquePtr json(cJSON_CreateObject());
        if (!json || !cJSON_AddStringToObject(json.get(), "motion", MotionName(motion)) ||
            !cJSON_AddNumberToObject(json.get(), "speed_percent", speed) ||
            !cJSON_AddNumberToObject(json.get(), "active_speed_percent", active_speed) ||
            !cJSON_AddBoolToObject(json.get(), "speed_pi_calibrated", speed_pi_calibrated) ||
            !cJSON_AddBoolToObject(json.get(), "speed_pi_active", speed_pi_active) ||
            !cJSON_AddNumberToObject(json.get(), "speed_pi_target_rpm", speed_pi_target_rpm) ||
            !cJSON_AddNumberToObject(json.get(), "left_pwm_percent", left_pwm_percent) ||
            !cJSON_AddNumberToObject(json.get(), "right_pwm_percent", right_pwm_percent) ||
            !cJSON_AddBoolToObject(json.get(), "emotion_motion_enabled", emotion_enabled) ||
            !cJSON_AddBoolToObject(json.get(), "emotion_motion_active", gesture_active) ||
            !cJSON_AddBoolToObject(json.get(), "web_manual_active", web_manual) ||
            !cJSON_AddStringToObject(json.get(), "autonomy_mode",
                                     AutonomyModeName(autonomy_mode)) ||
            !cJSON_AddBoolToObject(json.get(), "guard_enabled", guard_enabled) ||
            !cJSON_AddNumberToObject(json.get(), "guard_baseline_cm", guard_baseline_cm) ||
            !cJSON_AddStringToObject(json.get(), "scan_state", ScanStateName(scan_state)) ||
            !cJSON_AddNumberToObject(json.get(), "scan_left_cm", scan_left_cm) ||
            !cJSON_AddNumberToObject(json.get(), "scan_center_cm", scan_center_cm) ||
            !cJSON_AddNumberToObject(json.get(), "scan_right_cm", scan_right_cm) ||
            !cJSON_AddStringToObject(json.get(), "control_owner", control_owner) ||
            !cJSON_AddBoolToObject(json.get(), "warning_sound_enabled", warning_sound_enabled) ||
            !cJSON_AddStringToObject(json.get(), "route_state", RouteStateName(route_state)) ||
            !cJSON_AddStringToObject(json.get(), "route_script", route_script.data()) ||
            !cJSON_AddNumberToObject(json.get(), "route_step", route_step) ||
            !cJSON_AddNumberToObject(json.get(), "route_steps", route_steps) ||
            !cJSON_AddNumberToObject(json.get(), "follow_distance_cm", follow_distance) ||
            !cJSON_AddNumberToObject(json.get(), "obstacle_distance_cm", obstacle_distance) ||
            !cJSON_AddNumberToObject(json.get(), "follow_target_cm", effective_follow_distance) ||
            !cJSON_AddNumberToObject(json.get(), "obstacle_stop_cm", effective_obstacle_distance) ||
            !cJSON_AddNumberToObject(json.get(), "measured_distance_cm", measured_distance) ||
            !cJSON_AddBoolToObject(json.get(), "distance_valid", distance_valid) ||
            !cJSON_AddBoolToObject(json.get(), "autonomy_blocked", autonomy_blocked) ||
            !cJSON_AddBoolToObject(json.get(), "moving", moving) ||
            !cJSON_AddBoolToObject(json.get(), "pending", motion != Motion::kStop && !moving) ||
            !cJSON_AddNumberToObject(json.get(), "remaining_ms", remaining_ms) ||
            !cJSON_AddStringToObject(json.get(), "hardware_error", esp_err_to_name(fault)) ||
            !cJSON_AddStringToObject(json.get(), "distance_move_state",
                                     DistanceMoveStateName(distance_move_state)) ||
            !cJSON_AddNumberToObject(json.get(), "distance_move_target_cm",
                                     distance_move_target_cm) ||
            !cJSON_AddNumberToObject(json.get(), "distance_move_progress_cm",
                                     distance_move_progress_cm) ||
            !AddFc03EncoderJson(json.get(), encoder)) {
            return std::unexpected("Could not allocate motor status.");
        }
        return json.release();  // Ownership passes to MCP.
    }

    void RegisterTools() {
        auto& server = McpServer::GetInstance();
        const auto add_move = [this, &server](const char* name, const char* description,
                                              Motion motion) {
            const int default_duration_ms = motion == Motion::kLeft || motion == Motion::kRight
                                                ? kDefaultTurnDurationMs
                                                : kDefaultDurationMs;
            server.AddTool(
                name,
                std::string(description) + " duration_ms: 100..10000 milliseconds, default " +
                    std::to_string(default_duration_ms) +
                    "; 2 seconds = 2000. "
                    "speed_percent: 0..100 PWM duty, -1 keeps current speed (initially " +
                    std::to_string(kDefaultSpeedPercent) +
                    "). "
                    "Omit duration_ms and speed_percent unless the user specifies them; "
                    "use the firmware defaults instead of asking for these values. "
                    "Automatically stops locally. Replaces previous movement; does not queue. "
                    "Timed movement only; for centimetres use self.motor.forward_distance or "
                    "self.motor.backward_distance. "
                    "Result confirms acceptance, not completion. Do not repeat unless asked.",
                PropertyList({Property("duration_ms", kPropertyTypeInteger, default_duration_ms,
                                       100, kMaxDurationMs),
                              Property("speed_percent", kPropertyTypeInteger, -1, -1, 100)}),
                [this, motion](const PropertyList& properties) -> ToolResult {
                    return Move(motion, properties);
                });
        };
        add_move("self.motor.forward", "Move robot forward (tiến): both wheels forward.",
                 Motion::kForward);
        add_move("self.motor.backward", "Move robot backward (lùi): both wheels backward.",
                 Motion::kBackward);
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
        const auto add_distance_move = [this, &server](const char* name, const char* description,
                                                       Motion motion) {
            server.AddTool(
                name,
                std::string(description) +
                    " distance_cm is required (1..1000 cm of wheel travel, not HC-SR04 range). "
                    "Stops when each FC-03 wheel reaches the target distance. "
                    "speed_percent: 1..100 PWM duty, -1 keeps current speed. "
                    "Forward travel also stops near a detected obstacle. "
                    "Stops on missing pulses, encoder failure, or a distance-scaled timeout "
                    "(at least 10 seconds, 53 seconds maximum). Reverse travel has no rear sensor; "
                    "ensure the path is clear. "
                    "Result confirms acceptance, not completion; check distance_move_state and "
                    "distance_move_progress_cm via self.motor.get_state. Do not repeat unless "
                    "asked.",
                PropertyList({Property("distance_cm", kPropertyTypeInteger, 1, kMaxDistanceMoveCm),
                              Property("speed_percent", kPropertyTypeInteger, -1, -1, 100)}),
                [this, motion](const PropertyList& properties) -> ToolResult {
                    return MoveDistance(motion, properties);
                });
        };
        add_distance_move("self.motor.forward_distance",
                          "Drive forward (tiến) by a measured distance.", Motion::kForward);
        add_distance_move("self.motor.backward_distance",
                          "Drive backward (lùi) by a measured distance.", Motion::kBackward);
#endif
        add_move("self.motor.turn_left",
                 "Rotate robot left in place (xoay trái): left backward, right forward. No angle "
                 "feedback.",
                 Motion::kLeft);
        add_move("self.motor.turn_right",
                 "Rotate robot right in place (xoay phải): left forward, right backward. No angle "
                 "feedback.",
                 Motion::kRight);
        server.AddTool("self.motor.stop",
                       "Stop robot (dừng lại) now and cancel pending movement. Motors coast; speed "
                       "setting is retained. Also disables follow mode, obstacle avoidance, and "
                       "automatic emotion motion until "
                       "self.motor.set_emotion_motion is explicitly enabled again.",
                       PropertyList(), [this](const PropertyList&) -> ToolResult {
                           Stop();
                           return GetState();
                       });
        server.AddTool(
            "self.motor.set_speed",
            "Set motor PWM speed 0..100 percent (tốc độ). 0 stops. While moving, applies "
            "immediately to manual moves without extending duration; cancels an emotion gesture. "
            "While stopped, only saves speed for the next "
            "movement; never starts motion. This is not measured wheel speed.",
            PropertyList({Property("speed_percent", kPropertyTypeInteger, 0, 100)}),
            [this](const PropertyList& properties) -> ToolResult {
                return SetSpeed(properties["speed_percent"].value<int>());
            });
        server.AddTool("self.motor.get_state",
                       "Read commanded motor direction, PWM speed, pending/moving status, "
                       "remaining time and hardware error. Optional FC-03 encoder reports wheel "
                       "pulses, measured RPM, wheel distance and estimated robot travel since boot "
                       "in metres. Values are null until calibrated/ready. Direction is inferred "
                       "from commands; distance_complete=false means unknown-direction pulses "
                       "were excluded. Wheel slip and coasting cause error; no position feedback.",
                       PropertyList(),
                       [this](const PropertyList&) -> ToolResult { return GetState(); });
        server.AddTool(
            "self.motor.set_emotion_motion",
            "Enable/disable short automatic wheel gestures while the robot speaks "
            "(bật/tắt chuyển động theo cảm xúc). Enabling waits for a fresh expression; "
            "does not start motors immediately. Stop disables this feature. Defaults to enabled "
            "after reboot; not persisted. Requires a RobotEye display variant.",
            PropertyList({Property("enabled", kPropertyTypeBoolean)}),
            [this](const PropertyList& properties) -> ToolResult {
                return SetEmotionEnabled(properties["enabled"].value<bool>());
            });
        server.AddTool(
            "self.navigation.set_follow_mode",
            "Enable or disable HC-SR04 follow mode (bật/tắt chế độ đi theo). The robot only "
            "follows an object directly in front and stops when the target is lost. At 10 cm "
            "or closer it reverses until reaching the configured target distance; otherwise "
            "it moves forward when farther than the target plus a 4 cm deadband. The default "
            "target is 30 cm and does not change with speed. "
            "There is no rear obstacle sensor. Enabling disables obstacle avoidance. "
            "distance_cm: 10..200 changes the target before switching mode; -1 keeps the current "
            "target. Defaults off with a 30 cm target after reboot.",
            PropertyList({Property("enabled", kPropertyTypeBoolean),
                          Property("distance_cm", kPropertyTypeInteger, -1, -1,
                                   kMaxFollowDistanceCm)}),
            [this](const PropertyList& properties) -> ToolResult {
                const int distance = properties["distance_cm"].value<int>();
                if (distance != -1 && distance < kMinFollowDistanceCm)
                    return std::unexpected("Use follow distance 10..200 cm or -1 to keep it.");
                if (distance != -1 && SetFollowDistance(distance) != ESP_OK)
                    return std::unexpected("Use follow distance 10..200 cm or -1 to keep it.");
                const auto error = SetFollowMode(properties["enabled"].value<bool>());
                if (error != ESP_OK)
                    return std::unexpected(
                        "Follow mode is unavailable while web control, a motor fault, or zero "
                        "speed is active.");
                return GetState();
            });
        server.AddTool(
            "self.navigation.set_follow_distance",
            "Set the HC-SR04 follow target distance in centimetres (khoảng cách đi theo), "
            "10..200. Default is 30 cm and does not change with speed. Changing it "
            "does not enable follow mode.",
            PropertyList({Property("distance_cm", kPropertyTypeInteger, kMinFollowDistanceCm,
                                   kMaxFollowDistanceCm)}),
            [this](const PropertyList& properties) -> ToolResult {
                const auto error = SetFollowDistance(properties["distance_cm"].value<int>());
                if (error != ESP_OK)
                    return std::unexpected("Use follow distance 10..200 cm.");
                return GetState();
            });
        server.AddTool(
            "self.navigation.set_obstacle_avoidance",
            "Enable or disable autonomous HC-SR04 obstacle avoidance (bật/tắt tự hành né vật "
            "cản). At the effective stop distance the robot briefly reverses, probes left, and "
            "drives left immediately if clearance exceeds both 40 cm and the stop distance. "
            "Otherwise it probes right and chooses the farther safe direction, rechecking "
            "left if it returns there. It stops if neither direction is safe or readings fail. "
            "Turns are timed, not angle-controlled; there is no rear sensor. "
            "Enabling disables follow mode. Defaults "
            "off after reboot. distance_cm: 10..100 changes the stop distance before switching "
            "mode; -1 keeps the current distance (20 cm at 50% PWM after reboot). The effective "
            "stop distance scales proportionally above 50% and never falls below the configured "
            "value at lower speeds.",
            PropertyList({Property("enabled", kPropertyTypeBoolean),
                          Property("distance_cm", kPropertyTypeInteger, -1, -1,
                                   kMaxObstacleDistanceCm)}),
            [this](const PropertyList& properties) -> ToolResult {
                const int distance = properties["distance_cm"].value<int>();
                if (distance != -1 && distance < kMinObstacleDistanceCm)
                    return std::unexpected("Use obstacle distance 10..100 cm or -1 to keep it.");
                if (distance != -1 && SetObstacleDistance(distance) != ESP_OK)
                    return std::unexpected("Use obstacle distance 10..100 cm or -1 to keep it.");
                const auto error = SetObstacleAvoidance(properties["enabled"].value<bool>());
                if (error != ESP_OK)
                    return std::unexpected(
                        "Obstacle avoidance is unavailable while web control, a motor fault, or "
                        "zero speed is active.");
                return GetState();
            });
        server.AddTool(
            "self.navigation.set_obstacle_distance",
            "Set the HC-SR04 obstacle stop distance baseline in centimetres (khoảng cách né vật "
            "cản), 10..100. Default is 20 cm at 50% PWM; the effective stop distance scales "
            "proportionally above 50% and never falls below the configured value at lower speeds. "
            "Changing it does not enable obstacle avoidance.",
            PropertyList({Property("distance_cm", kPropertyTypeInteger, kMinObstacleDistanceCm,
                                   kMaxObstacleDistanceCm)}),
            [this](const PropertyList& properties) -> ToolResult {
                const auto error = SetObstacleDistance(properties["distance_cm"].value<int>());
                if (error != ESP_OK)
                    return std::unexpected("Use obstacle distance 10..100 cm.");
                return GetState();
            });
        server.AddTool("self.navigation.get_state",
                       "Read HC-SR04 distance, sensor validity, configured and speed-adjusted "
                       "follow/avoid thresholds, current autonomy mode and blocked state.",
                       PropertyList(),
                       [this](const PropertyList&) -> ToolResult { return GetState(); });
        server.AddTool(
            "self.navigation.set_guard_mode",
            "Enable or disable stationary HC-SR04 guard mode. Three sustained readings at least "
            "20 cm from its baseline trigger a screen, speaker and configured Telegram alert. "
            "Alerts have a 30 second cooldown. Monitoring pauses while the robot moves. "
            "Enabling requires the robot to be stopped and outside web/autonomous control.",
            PropertyList({Property("enabled", kPropertyTypeBoolean)}),
            [this](const PropertyList& properties) -> ToolResult {
                if (SetGuardMode(properties["enabled"].value<bool>()) != ESP_OK)
                    return std::unexpected("Guard mode requires an idle robot.");
                return GetState();
            });
        server.AddTool(
            "self.navigation.scan_space",
            "Start a bounded timed left/center/right HC-SR04 scan, then turn back to the "
            "estimated starting heading. Requires a stopped robot outside web/autonomous "
            "control. Turn angles are not measured; results are an approximate clearance "
            "diagram, not a map. Read scan_state and scan distances via web status.",
            PropertyList(), [this](const PropertyList&) -> ToolResult {
                if (StartScan() != ESP_OK)
                    return std::unexpected("Space scan requires an idle robot.");
                return GetState();
            });
        server.AddTool(
            "self.navigation.run_route",
            "Run a bounded multi-step route. Script syntax: F30,L15,S means forward 30 cm, "
            "turn left using 15 cm of wheel travel, then stop. B and R mean backward and "
            "right. Each step is 1..50 cm, up to five moves. FC-03 calibration is required. "
            "Turns are approximate; the rear has no obstacle sensor. Stop interrupts it at "
            "any time. The script remains saved in RAM for web or Telegram replay.",
            PropertyList({Property("script", kPropertyTypeString).SetMaxLength(63)}),
            [this](const PropertyList& properties) -> ToolResult {
                const auto script = properties["script"].value<std::string>();
                if (SaveRoute(script.c_str()) != ESP_OK)
                    return std::unexpected("Use up to five F/B/L/R steps of 1..50 cm, optionally ending in S.");
                if (RunRoute("voice") != ESP_OK)
                    return std::unexpected("Route needs an idle robot and calibrated FC-03 encoders.");
                return GetState();
            });
    }
};

}  // namespace

void InitializeL298nController() {
    // Singleton and tool callbacks live for the entire firmware lifetime.
    L298nController::Instance();
    StartFc03Monitor([]() { return L298nController::Instance().Snapshot().encoder; });
    InitializeHcsr04Sensor([](int distance_cm, bool valid) {
        auto& controller = L298nController::Instance();
        controller.UpdateDistance(distance_cm, valid);
        controller.ScheduleGuardAlert();
        controller.ScheduleProximityWarning();
    });
    ESP_ERROR_CHECK(esp_register_shutdown_handler([]() { L298nController::Instance().Stop(); }));
}

void UpdateL298nEmotion(const char* emotion, bool speaking) {
    L298nController::Instance().UpdateEmotion(emotion, speaking);
}

esp_err_t BeginL298nWebControl(uint32_t* session) {
    return L298nController::Instance().BeginWebControl(session);
}

esp_err_t CommandL298nWebControl(uint32_t session, uint32_t sequence, L298nWebCommand command,
                                 int speed_percent, bool proximity_warning_enabled,
                                 int joystick_x, int joystick_y) {
    return L298nController::Instance().WebCommand(session, sequence, command, speed_percent,
                                                 proximity_warning_enabled, joystick_x,
                                                 joystick_y);
}

void SetL298nWebConnected(bool connected) {
    L298nController::Instance().SetWebConnected(connected);
}

L298nMotorStatus GetL298nMotorStatus() { return L298nController::Instance().Snapshot(); }

esp_err_t SetL298nMotorSpeed(int speed_percent) {
    return L298nController::Instance().SetSpeedPercent(speed_percent);
}

esp_err_t MoveL298nDistance(bool forward, int distance_cm) {
    return L298nController::Instance().MoveDistanceFromCommand(forward, distance_cm);
}

esp_err_t SetL298nFollowMode(bool enabled) {
    return L298nController::Instance().SetFollowMode(enabled);
}

esp_err_t SetL298nObstacleAvoidance(bool enabled) {
    return L298nController::Instance().SetObstacleAvoidance(enabled);
}

esp_err_t SetL298nFollowDistance(int distance_cm) {
    return L298nController::Instance().SetFollowDistance(distance_cm);
}

esp_err_t SetL298nObstacleDistance(int distance_cm) {
    return L298nController::Instance().SetObstacleDistance(distance_cm);
}

esp_err_t SetL298nGuardMode(bool enabled) {
    return L298nController::Instance().SetGuardMode(enabled);
}

esp_err_t StartL298nSpaceScan() { return L298nController::Instance().StartScan(); }

void CancelL298nSpaceScan() { L298nController::Instance().CancelScan(); }

esp_err_t SaveL298nRoute(const char* script) {
    return L298nController::Instance().SaveRoute(script);
}

esp_err_t RunL298nRoute(const char* owner) {
    return L298nController::Instance().RunRoute(owner);
}

void CancelL298nRoute() { L298nController::Instance().CancelRoute(); }

void StopL298nMotor() { L298nController::Instance().Stop(); }
