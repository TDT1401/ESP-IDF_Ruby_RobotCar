#include "l298n_controller.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_err.h>
#include <esp_pm.h>
#include <esp_system.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstdint>
#include <mutex>

#include "../config.h"
#include "application.h"
#include "mcp_server.h"

namespace {

constexpr auto kMode = LEDC_LOW_SPEED_MODE;
constexpr auto kTimer = LEDC_TIMER_1;  // LCD backlight owns timer/channel 0.
constexpr auto kLeftChannel = LEDC_CHANNEL_1;
constexpr auto kRightChannel = LEDC_CHANNEL_2;
constexpr int kDefaultDurationMs = 1000;
constexpr int kMaxDurationMs = 10000;
constexpr int64_t kDirectionDelayUs = 50000;
constexpr int64_t kTickUs = 10000;

enum class Motion { kStop, kForward, kBackward, kLeft, kRight };

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

class L298nController {
public:
    static L298nController& Instance() {
        static L298nController controller;
        return controller;
    }

    void Stop() {
        std::lock_guard lock(mutex_);
        StopLocked();
    }

private:
    // MCP runs on the application task; the timer also accesses this controller.
    // Only short state/GPIO operations hold the mutex: no sleeps, JSON, logging,
    // networking or Application::Schedule calls inside it.
    std::mutex mutex_;
    esp_timer_handle_t timer_ = nullptr;
#ifdef CONFIG_PM_ENABLE
    esp_pm_lock_handle_t sleep_lock_ = nullptr;
    bool sleep_lock_held_ = false;
#endif
    Motion motion_ = Motion::kStop;
    int speed_percent_ = 50;
    int duration_ms_ = kDefaultDurationMs;
    int64_t start_at_us_ = 0;
    int64_t deadline_us_ = 0;
    bool moving_ = false;
    esp_err_t fault_ = ESP_OK;

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

    void StopLocked() {
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
        const uint32_t duty = speed_percent_ * 1023 / 100;
        // These channels have no fade users and all accesses are serialized.
        return Check(ledc_set_duty(kMode, kLeftChannel, duty)) &&
               Check(ledc_update_duty(kMode, kLeftChannel)) &&
               Check(ledc_set_duty(kMode, kRightChannel, duty)) &&
               Check(ledc_update_duty(kMode, kRightChannel));
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

    void Tick() {
        std::lock_guard lock(mutex_);
        if (motion_ == Motion::kStop) {
            return;
        }
        const auto state = Application::GetInstance().GetDeviceState();
        const int64_t now = esp_timer_get_time();
        if (now >= deadline_us_ || state == kDeviceStateUpgrading ||
            state == kDeviceStateFatalError || state == kDeviceStateWifiConfiguring) {
            StopLocked();
        } else if (!moving_ && now >= start_at_us_) {
            if (!ApplyDirectionLocked() || !ApplySpeedLocked()) {
                StopLocked();
                return;
            }
            moving_ = true;
            // Count the requested duration from enabling the motors. A delayed
            // timer must never start a command that has already expired above.
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
#ifdef CONFIG_PM_ENABLE
                if (!Check(esp_pm_lock_acquire(sleep_lock_))) {
                    return std::unexpected("Could not keep the motor deadline timer awake.");
                }
                sleep_lock_held_ = true;
#endif
                motion_ = motion;
                duration_ms_ = duration;
                start_at_us_ = esp_timer_get_time() + kDirectionDelayUs;
                deadline_us_ = start_at_us_ + static_cast<int64_t>(duration) * 1000;
            }
        }
        return GetState();
    }

    ToolResult SetSpeed(int speed) {
        if (speed < 0 || speed > 100) {
            return std::unexpected("Use speed_percent 0..100.");
        }
        {
            std::lock_guard lock(mutex_);
            speed_percent_ = speed;
            if (speed == 0 || (motion_ != Motion::kStop && esp_timer_get_time() >= deadline_us_)) {
                StopLocked();
            } else if (moving_ && !ApplySpeedLocked()) {
                StopLocked();
            }
            // Changing speed never extends the deadline or restarts stopped motors.
            if (fault_ != ESP_OK) {
                return std::unexpected("Motor hardware error; inspect wiring and restart.");
            }
        }
        return GetState();
    }

    ToolResult GetState() {
        Motion motion;
        int speed;
        bool moving;
        int64_t remaining_ms;
        esp_err_t fault;
        {
            std::lock_guard lock(mutex_);
            motion = motion_;
            speed = speed_percent_;
            moving = moving_;
            fault = fault_;
            remaining_ms =
                motion == Motion::kStop ? 0
                : moving ? std::max<int64_t>(0, (deadline_us_ - esp_timer_get_time() + 999) / 1000)
                         : duration_ms_;
        }
        CJsonUniquePtr json(cJSON_CreateObject());
        if (!json || !cJSON_AddStringToObject(json.get(), "motion", MotionName(motion)) ||
            !cJSON_AddNumberToObject(json.get(), "speed_percent", speed) ||
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
            server.AddTool(
                name,
                std::string(description) +
                    " duration_ms: 100..10000 milliseconds, default 1000; 2 seconds = 2000. "
                    "speed_percent: 0..100 PWM duty, -1 keeps current speed (initially 50). "
                    "Automatically stops locally. Replaces previous movement; does not queue. "
                    "Result confirms acceptance, not completion. Do not repeat unless asked.",
                PropertyList({Property("duration_ms", kPropertyTypeInteger, kDefaultDurationMs, 100,
                                       kMaxDurationMs),
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
                       "setting is retained.",
                       PropertyList(), [this](const PropertyList&) -> ToolResult {
                           Stop();
                           return GetState();
                       });
        server.AddTool(
            "self.motor.set_speed",
            "Set motor PWM speed 0..100 percent (tốc độ). 0 stops. While moving, applies "
            "immediately without extending duration. While stopped, only saves speed for the next "
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
    }
};

}  // namespace

void InitializeL298nController() {
    // Singleton and tool callbacks live for the entire firmware lifetime.
    L298nController::Instance();
    ESP_ERROR_CHECK(esp_register_shutdown_handler([]() { L298nController::Instance().Stop(); }));
}
