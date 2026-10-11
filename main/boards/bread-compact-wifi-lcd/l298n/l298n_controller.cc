#include "l298n_controller.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_err.h>
#include <esp_pm.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

#include "../config.h"
#include "application.h"
#include "hcsr04_sensor.h"
#include "mcp_server.h"

namespace {

constexpr auto kMode = LEDC_LOW_SPEED_MODE;
constexpr auto kTimer = LEDC_TIMER_1;  // LCD backlight owns timer/channel 0.
constexpr auto kLeftChannel = LEDC_CHANNEL_1;
constexpr auto kRightChannel = LEDC_CHANNEL_2;
constexpr int kDefaultSpeedPercent = 75;
constexpr int kDefaultDurationMs = 1000;
constexpr int kDefaultTurnDurationMs = 400;
constexpr int kMaxDurationMs = 10000;
constexpr int64_t kDirectionDelayUs = 50000;
constexpr int64_t kTickUs = 10000;
constexpr int64_t kGestureMaxUs = 2000000;
constexpr int64_t kGestureCooldownUs = 3000000;
constexpr int kWebTimeoutMs = 600;
constexpr int kDefaultFollowDistanceCm = 30;
constexpr int kDefaultObstacleDistanceCm = 20;
constexpr int kMinFollowDistanceCm = 10;
constexpr int kMaxFollowDistanceCm = 200;
constexpr int kMinObstacleDistanceCm = 10;
constexpr int kMaxObstacleDistanceCm = 100;
constexpr int kFollowMaxTargetCm = 300;
constexpr int kFollowHysteresisCm = 4;
constexpr int kAvoidClearanceCm = 10;
constexpr int kAutonomyCommandMs = 180;
constexpr int kAvoidTurnTimeoutMs = 900;
constexpr int kSensorStaleMs = 250;
constexpr int kAutonomySpeedPercent = 40;
constexpr int kAutonomyTurnSpeedPercent = 38;

enum class Motion { kStop, kForward, kBackward, kLeft, kRight };
enum class Emotion { kNone, kHappy, kSad, kAngry, kSurprised, kThinking, kWinking };
enum class AutonomyMode { kManual, kFollow, kObstacleAvoidance };
enum class AvoidState { kForward, kTurnFirst, kTurnSecond, kBlocked };

struct GestureStep {
    Motion motion;
    int duration_ms;
    int speed_percent;
};

// Timed gestures, not position-controlled moves. Each direction change includes a 50 ms stop.
constexpr GestureStep kHappyGesture[] = {{Motion::kForward, 180, 55},
                                         {Motion::kBackward, 180, 55},
                                         {Motion::kLeft, 140, 55},
                                         {Motion::kRight, 280, 55},
                                         {Motion::kLeft, 140, 55}};
constexpr GestureStep kSadGesture[] = {{Motion::kBackward, 180, 40}};
constexpr GestureStep kSurprisedGesture[] = {{Motion::kBackward, 140, 50}};
constexpr GestureStep kAngryGesture[] = {
    {Motion::kLeft, 100, 50}, {Motion::kRight, 200, 50}, {Motion::kLeft, 100, 50}};
constexpr GestureStep kThinkingGesture[] = {
    {Motion::kLeft, 120, 45}, {Motion::kRight, 240, 45}, {Motion::kLeft, 120, 45}};
constexpr GestureStep kWinkingGesture[] = {
    {Motion::kLeft, 160, 55}, {Motion::kRight, 320, 55}, {Motion::kLeft, 160, 55}};

template <size_t N>
constexpr bool IsValidGesture(const GestureStep (&steps)[N]) {
    int64_t total_us = 0;
    for (const auto& step : steps) {
        if (step.motion == Motion::kStop || step.duration_ms < 100 || step.duration_ms > 400 ||
            step.speed_percent <= 0 || step.speed_percent > 60)
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

class L298nController {
public:
    static L298nController& Instance() {
        static L298nController controller;
        return controller;
    }

    void Stop() {
        std::lock_guard lock(mutex_);
        // A spoken stop must not be undone by an emotion in the acknowledgement.
        emotion_enabled_ = false;
        pending_emotion_ = Emotion::kNone;
        manual_override_ = true;
        DisableAutonomyLocked();
        web_session_ = 0;  // A spoken stop also invalidates in-flight browser commands.
        web_sequence_ = 0;
        web_deadline_us_ = 0;
        web_manual_ = false;
        StopLocked();
    }

    esp_err_t SetSpeedPercent(int speed) {
        if (speed < 0 || speed > 100)
            return ESP_ERR_INVALID_ARG;
        std::lock_guard lock(mutex_);
        if (web_manual_)
            return ESP_ERR_INVALID_STATE;
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

    L298nMotorStatus Snapshot() {
        std::lock_guard lock(mutex_);
        const bool distance_fresh = sensor_valid_ &&
                                    esp_timer_get_time() - last_sensor_update_us_ <=
                                        kSensorStaleMs * 1000LL;
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
                distance_fresh ? measured_distance_cm_ : -1,
                distance_fresh,
                avoid_state_ == AvoidState::kBlocked,
                fault_};
    }

    void SetWebConnected(bool connected) {
        std::lock_guard lock(mutex_);
        web_connected_ = connected;
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
        DisableAutonomyLocked();
        StopLocked();
        if (fault_ != ESP_OK)
            return fault_;
        pending_emotion_ = Emotion::kNone;
        web_manual_ = true;
        web_sequence_ = 0;
        // Zero is reserved for an expired/released session.
        web_session_ = esp_random();
        if (web_session_ == 0)
            web_session_ = 1;
        web_deadline_us_ = now + kWebTimeoutMs * 1000LL;
        *session = web_session_;
        return ESP_OK;
    }

    esp_err_t WebCommand(uint32_t session, uint32_t sequence, L298nWebCommand command, int speed) {
        if (speed < 0 || speed > 100)
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
        } else {
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
        std::lock_guard lock(mutex_);
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
        if (emotion_enabled_ && !manual_override_ && !web_manual_ &&
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

    void UpdateDistance(int distance_cm, bool valid) {
        std::lock_guard lock(mutex_);
        last_sensor_update_us_ = esp_timer_get_time();
        sensor_valid_ = valid;
        measured_distance_cm_ = valid ? distance_cm : -1;
        if (!valid) {
            valid_sensor_samples_ = 0;
            if (++invalid_sensor_samples_ >= 2 && autonomy_mode_ != AutonomyMode::kManual)
                StopLocked();
            return;
        }
        invalid_sensor_samples_ = 0;
        valid_sensor_samples_ = std::min(valid_sensor_samples_ + 1, 3);
        if (autonomy_mode_ == AutonomyMode::kManual || web_manual_ || fault_ != ESP_OK ||
            !MotionAllowed()) {
            return;
        }
        if (valid_sensor_samples_ < 2) {
            StopLocked();
            return;
        }
        if (autonomy_mode_ == AutonomyMode::kFollow)
            UpdateFollowLocked(distance_cm);
        else
            UpdateObstacleAvoidanceLocked(distance_cm);
    }

private:
    // MCP, HTTP and network callbacks and the timer share this controller.
    // Only short state/GPIO operations hold the mutex: no sleeps, JSON, logging,
    // networking or Application::Schedule calls inside it.
    std::mutex mutex_;
    esp_timer_handle_t timer_ = nullptr;
#ifdef CONFIG_PM_ENABLE
    esp_pm_lock_handle_t sleep_lock_ = nullptr;
    bool sleep_lock_held_ = false;
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
    uint32_t web_session_ = 0;
    uint32_t web_sequence_ = 0;
    int64_t web_deadline_us_ = 0;
    int web_speed_percent_ = kDefaultSpeedPercent;
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
    int64_t avoid_turn_started_us_ = 0;
    bool sensor_valid_ = false;
    bool next_avoid_turn_left_ = true;

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
#endif
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
        avoid_turn_started_us_ = 0;
        valid_sensor_samples_ = 0;
        invalid_sensor_samples_ = 0;
    }

    esp_err_t SetAutonomyModeLocked(AutonomyMode requested, AutonomyMode controlled_mode) {
        if (requested == AutonomyMode::kManual && autonomy_mode_ != controlled_mode)
            return ESP_OK;
        if (web_manual_)
            return ESP_ERR_INVALID_STATE;
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
        avoid_turn_started_us_ = 0;
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
        avoid_state_ = AvoidState::kForward;
        if (distance_cm <= follow_distance_cm_ + kFollowHysteresisCm ||
            distance_cm > kFollowMaxTargetCm) {
            StopLocked();
            return;
        }
        RefreshAutonomyMotionLocked(Motion::kForward, kAutonomySpeedPercent);
    }

    void StartAvoidTurnLocked(AvoidState state) {
        StopLocked();
        avoid_state_ = state;
        avoid_clear_samples_ = 0;
        avoid_turn_started_us_ = esp_timer_get_time();
        const bool turn_left = state == AvoidState::kTurnFirst ? next_avoid_turn_left_
                                                               : !next_avoid_turn_left_;
        RefreshAutonomyMotionLocked(turn_left ? Motion::kLeft : Motion::kRight,
                                    kAutonomyTurnSpeedPercent);
    }

    void UpdateObstacleAvoidanceLocked(int distance_cm) {
        const int clear_distance = obstacle_distance_cm_ + kAvoidClearanceCm;
        const int64_t now = esp_timer_get_time();
        if (avoid_state_ == AvoidState::kBlocked) {
            if (distance_cm > clear_distance) {
                if (++avoid_clear_samples_ >= 3) {
                    avoid_state_ = AvoidState::kForward;
                    avoid_clear_samples_ = 0;
                    RefreshAutonomyMotionLocked(Motion::kForward, kAutonomySpeedPercent);
                }
            } else {
                avoid_clear_samples_ = 0;
                StopLocked();
            }
            return;
        }
        if (avoid_state_ == AvoidState::kForward) {
            if (distance_cm <= obstacle_distance_cm_) {
                StartAvoidTurnLocked(AvoidState::kTurnFirst);
            } else {
                RefreshAutonomyMotionLocked(Motion::kForward, kAutonomySpeedPercent);
            }
            return;
        }

        if (distance_cm > clear_distance) {
            ++avoid_clear_samples_;
        } else {
            avoid_clear_samples_ = 0;
        }
        if (avoid_clear_samples_ >= 2) {
            next_avoid_turn_left_ = !next_avoid_turn_left_;
            avoid_state_ = AvoidState::kForward;
            avoid_clear_samples_ = 0;
            RefreshAutonomyMotionLocked(Motion::kForward, kAutonomySpeedPercent);
            return;
        }
        if (now - avoid_turn_started_us_ >= kAvoidTurnTimeoutMs * 1000LL) {
            if (avoid_state_ == AvoidState::kTurnFirst) {
                StartAvoidTurnLocked(AvoidState::kTurnSecond);
            } else {
                StopLocked();
                avoid_state_ = AvoidState::kBlocked;
                next_avoid_turn_left_ = !next_avoid_turn_left_;
            }
            return;
        }
        const bool turn_left = avoid_state_ == AvoidState::kTurnFirst ? next_avoid_turn_left_
                                                                      : !next_avoid_turn_left_;
        RefreshAutonomyMotionLocked(turn_left ? Motion::kLeft : Motion::kRight,
                                    kAutonomyTurnSpeedPercent);
    }

    void StopLocked() {
        gesture_steps_ = nullptr;
        gesture_count_ = 0;
        gesture_index_ = 0;
        gesture_deadline_us_ = 0;
        StopOutputsLocked();
    }

    void EndWebControlLocked() {
        web_session_ = 0;
        web_sequence_ = 0;
        web_deadline_us_ = 0;
        web_manual_ = false;
        pending_emotion_ = Emotion::kNone;
        // Do not replay a gesture from the response that was active while the browser owned it.
        manual_override_ = response_active_;
        StopLocked();
    }

    void StopOutputsLocked() {
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
        start_at_us_ = 0;
        deadline_us_ = 0;
#ifdef CONFIG_PM_ENABLE
        if (sleep_lock_held_) {
            Check(esp_pm_lock_release(sleep_lock_));
            sleep_lock_held_ = false;
        }
#endif
    }

    bool ApplySpeedLocked() {
        const uint32_t duty = ActiveSpeedLocked() * 1023 / 100;
        // These channels have no fade users and all accesses are serialized.
        return Check(ledc_set_duty(kMode, kLeftChannel, duty)) &&
               Check(ledc_update_duty(kMode, kLeftChannel)) &&
               Check(ledc_set_duty(kMode, kRightChannel, duty)) &&
               Check(ledc_update_duty(kMode, kRightChannel));
    }

    int ActiveSpeedLocked() const {
        if (web_manual_)
            return web_speed_percent_;
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
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_LEFT
        left_forward = !left_forward;
#endif
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_RIGHT
        right_forward = !right_forward;
#endif
        return Check(gpio_set_level(MOTOR_LEFT_IN1_PIN, left_forward)) &&
               Check(gpio_set_level(MOTOR_LEFT_IN2_PIN, !left_forward)) &&
               Check(gpio_set_level(MOTOR_RIGHT_IN1_PIN, right_forward)) &&
               Check(gpio_set_level(MOTOR_RIGHT_IN2_PIN, !right_forward));
    }

    static bool MotionAllowed() {
        const auto state = Application::GetInstance().GetDeviceState();
        return state != kDeviceStateUpgrading && state != kDeviceStateFatalError &&
               state != kDeviceStateWifiConfiguring;
    }

    void Tick() {
        std::lock_guard lock(mutex_);
        const int64_t now = esp_timer_get_time();
        if (web_session_ != 0 && (now >= web_deadline_us_ || !MotionAllowed())) {
            EndWebControlLocked();
        }
        const auto state = Application::GetInstance().GetDeviceState();
        if (state == kDeviceStateUpgrading || state == kDeviceStateFatalError ||
            state == kDeviceStateWifiConfiguring) {
            pending_emotion_ = Emotion::kNone;
            DisableAutonomyLocked();
            if (motion_ != Motion::kStop)
                StopLocked();
            return;
        }
        if (autonomy_mode_ != AutonomyMode::kManual &&
            (invalid_sensor_samples_ >= 2 ||
             now - last_sensor_update_us_ > kSensorStaleMs * 1000LL)) {
            if (motion_ != Motion::kStop)
                StopLocked();
            return;
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
            if (!emotion_enabled_ || manual_override_ || web_manual_ || !response_active_ ||
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
                StopLocked();
            }
        } else if (!moving_ && now >= start_at_us_) {
            if (!ApplyDirectionLocked() || !ApplySpeedLocked()) {
                StopLocked();
                return;
            }
            moving_ = true;
            // Count the requested duration from enabling the motors. A delayed
            // timer must never start a command that has already expired above.
            // Web deadlines are measured from reception, never from motor start.
            if (!web_manual_)
                deadline_us_ = esp_timer_get_time() + static_cast<int64_t>(duration_ms_) * 1000;
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
            DisableAutonomyLocked();
            manual_override_ = true;
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
        int measured_distance;
        bool moving;
        bool emotion_enabled;
        bool gesture_active;
        bool web_manual;
        bool distance_valid;
        bool autonomy_blocked;
        int64_t remaining_ms;
        esp_err_t fault;
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
            distance_valid =
                sensor_valid_ && now - last_sensor_update_us_ <= kSensorStaleMs * 1000LL;
            measured_distance = distance_valid ? measured_distance_cm_ : -1;
            autonomy_blocked = avoid_state_ == AvoidState::kBlocked;
            emotion_enabled = emotion_enabled_;
            gesture_active = gesture_steps_ != nullptr;
            web_manual = web_manual_;
            fault = fault_;
            remaining_ms =
                motion == Motion::kStop ? 0
                : (moving || web_manual)
                    ? std::max<int64_t>(0, (deadline_us_ - now + 999) / 1000)
                    : duration_ms_;
        }
        CJsonUniquePtr json(cJSON_CreateObject());
        if (!json || !cJSON_AddStringToObject(json.get(), "motion", MotionName(motion)) ||
            !cJSON_AddNumberToObject(json.get(), "speed_percent", speed) ||
            !cJSON_AddNumberToObject(json.get(), "active_speed_percent", active_speed) ||
            !cJSON_AddBoolToObject(json.get(), "emotion_motion_enabled", emotion_enabled) ||
            !cJSON_AddBoolToObject(json.get(), "emotion_motion_active", gesture_active) ||
            !cJSON_AddBoolToObject(json.get(), "web_manual_active", web_manual) ||
            !cJSON_AddStringToObject(json.get(), "autonomy_mode",
                                    AutonomyModeName(autonomy_mode)) ||
            !cJSON_AddNumberToObject(json.get(), "follow_distance_cm", follow_distance) ||
            !cJSON_AddNumberToObject(json.get(), "obstacle_distance_cm", obstacle_distance) ||
            !cJSON_AddNumberToObject(json.get(), "measured_distance_cm", measured_distance) ||
            !cJSON_AddBoolToObject(json.get(), "distance_valid", distance_valid) ||
            !cJSON_AddBoolToObject(json.get(), "autonomy_blocked", autonomy_blocked) ||
            !cJSON_AddBoolToObject(json.get(), "moving", moving) ||
            !cJSON_AddBoolToObject(json.get(), "pending", motion != Motion::kStop && !moving) ||
            !cJSON_AddNumberToObject(json.get(), "remaining_ms", remaining_ms) ||
            !cJSON_AddStringToObject(json.get(), "hardware_error", esp_err_to_name(fault))) {
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
                       "remaining time and hardware error. No encoder feedback.",
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
            "follows an object directly in front, moves forward only, and stops when the target "
            "is lost or reaches the configured distance. Enabling disables obstacle avoidance. "
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
            "Set the HC-SR04 follow target distance in centimetres (khoảng cách đi theo), 10..200. "
            "Default is 30 cm. Changing it does not enable follow mode.",
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
            "cản). The robot moves forward, turns in place when blocked, never reverses, and "
            "stops if both turn attempts remain blocked. Enabling disables follow mode. Defaults "
            "off after reboot. distance_cm: 10..100 changes the stop distance before switching "
            "mode; -1 keeps the current distance (20 cm after reboot).",
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
            "Set the HC-SR04 obstacle stop distance in centimetres (khoảng cách né vật cản), "
            "10..100. Default is 20 cm. Changing it does not enable obstacle avoidance.",
            PropertyList({Property("distance_cm", kPropertyTypeInteger, kMinObstacleDistanceCm,
                                   kMaxObstacleDistanceCm)}),
            [this](const PropertyList& properties) -> ToolResult {
                const auto error = SetObstacleDistance(properties["distance_cm"].value<int>());
                if (error != ESP_OK)
                    return std::unexpected("Use obstacle distance 10..100 cm.");
                return GetState();
            });
        server.AddTool("self.navigation.get_state",
                       "Read HC-SR04 distance, sensor validity, follow/avoid thresholds, current "
                       "autonomy mode and blocked state.",
                       PropertyList(),
                       [this](const PropertyList&) -> ToolResult { return GetState(); });
    }
};

}  // namespace

void InitializeL298nController() {
    // Singleton and tool callbacks live for the entire firmware lifetime.
    L298nController::Instance();
    InitializeHcsr04Sensor([](int distance_cm, bool valid) {
        L298nController::Instance().UpdateDistance(distance_cm, valid);
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
                                 int speed_percent) {
    return L298nController::Instance().WebCommand(session, sequence, command, speed_percent);
}

void SetL298nWebConnected(bool connected) {
    L298nController::Instance().SetWebConnected(connected);
}

L298nMotorStatus GetL298nMotorStatus() { return L298nController::Instance().Snapshot(); }

esp_err_t SetL298nMotorSpeed(int speed_percent) {
    return L298nController::Instance().SetSpeedPercent(speed_percent);
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

void StopL298nMotor() { L298nController::Instance().Stop(); }
