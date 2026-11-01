#include "controller_internal.h"

namespace ruby_robot {

const char* L298nController::ControlOwnerLocked() const {
    if (web_manual_)
        return "web";
    if (autonomy_mode_ != AutonomyMode::kManual)
        return "automatic";
    return motion_ != Motion::kStop ? control_owner_ : "none";
}

L298nController& L298nController::Instance() {
    static L298nController controller;
    return controller;
}

void L298nController::Stop() {
    std::lock_guard lock(mutex_);
    manual_override_ = true;
    DisableAutonomyLocked();
    web_session_ = 0;  // A spoken stop also invalidates in-flight browser commands.
    web_sequence_ = 0;
    web_deadline_us_ = 0;
    web_manual_ = false;
    ReleaseWebSleepLockLocked();
    StopLocked();
}

esp_err_t L298nController::SetSpeedPercent(int speed) {
    if (speed < 0 || speed > 100)
        return ESP_ERR_INVALID_ARG;
    std::lock_guard lock(mutex_);
    if (web_manual_)
        return ESP_ERR_INVALID_STATE;
    manual_override_ = true;
    speed_percent_ = speed;
    if (speed == 0)
        DisableAutonomyLocked();
    if (speed == 0 ||
        (deadline_us_ > 0 && motion_ != Motion::kStop && esp_timer_get_time() >= deadline_us_)) {
        StopLocked();
    } else if (moving_ && !ApplySpeedLocked()) {
        StopLocked();
    }
    // Changing speed never extends a safety deadline or restarts stopped motors.
    // Keep hardware faults distinct from argument/ownership errors; Snapshot retains details.
    return fault_ == ESP_OK ? ESP_OK : ESP_FAIL;
}

L298nMotorStatus L298nController::Snapshot() {
    std::lock_guard lock(mutex_);
    const bool distance_fresh =
        sensor_valid_ && esp_timer_get_time() - last_sensor_update_us_ <= kSensorStaleMs * 1000LL;
    return {MotionName(motion_),
            speed_percent_,
            moving_ ? ActiveSpeedLocked() : 0,
            moving_,
            motion_ != Motion::kStop && !moving_,
            web_manual_,
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
            turn_target_degrees_,
            turn_progress_degrees_,
            imu_ready_,
            GetMpu6050Status().error,
            imu_axis_mismatch_,
            yaw_degrees_,
            gyro_z_dps_,
            heading_hold_active_ && imu_ready_,
            turn_uses_imu_,
            kSpeedPiCalibrationValid,
            speed_pi_active_,
            speed_pi_target_rpm_,
            CurrentLeftPwmPercentLocked(),
            CurrentRightPwmPercentLocked(),
            ControlOwnerLocked(),
            proximity_warning_enabled_};
}

L298nController::L298nController() {
    constexpr uint64_t pins = (1ULL << MOTOR_LEFT_EN_PIN) | (1ULL << MOTOR_LEFT_IN1_PIN) |
                              (1ULL << MOTOR_LEFT_IN2_PIN) | (1ULL << MOTOR_RIGHT_EN_PIN) |
                              (1ULL << MOTOR_RIGHT_IN1_PIN) | (1ULL << MOTOR_RIGHT_IN2_PIN);
    gpio_config_t gpio = {};
    gpio.pin_bit_mask = pins;
    gpio.mode = GPIO_MODE_OUTPUT;
    gpio.pull_down_en = GPIO_PULLDOWN_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&gpio));
    for (auto pin : {MOTOR_LEFT_EN_PIN, MOTOR_LEFT_IN1_PIN, MOTOR_LEFT_IN2_PIN, MOTOR_RIGHT_EN_PIN,
                     MOTOR_RIGHT_IN1_PIN, MOTOR_RIGHT_IN2_PIN}) {
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
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "l298n_web", &web_sleep_lock_));
#endif
    encoder_.Initialize();
    InitializeMpu6050();
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

L298nController::~L298nController() {
    esp_timer_stop(timer_);
    esp_timer_delete(timer_);
    Stop();
#ifdef CONFIG_PM_ENABLE
    esp_pm_lock_delete(sleep_lock_);
    esp_pm_lock_delete(web_sleep_lock_);
#endif
}

bool L298nController::Check(esp_err_t error) {
    if (error != ESP_OK && fault_ == ESP_OK) {
        fault_ = error;  // Latch hardware errors until reboot; never auto-resume.
    }
    return error == ESP_OK;
}

}  // namespace ruby_robot

using ruby_robot::L298nController;

void InitializeL298nController() {
    // Singleton and tool callbacks live for the entire firmware lifetime.
    L298nController::Instance();
    StartFc03Monitor([]() { return L298nController::Instance().Snapshot().encoder; });
    InitializeHcsr04Sensor([](int distance_cm, bool valid) {
        auto& controller = L298nController::Instance();
        controller.UpdateDistance(distance_cm, valid);
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
                                 int speed_percent, bool proximity_warning_enabled, int joystick_x,
                                 int joystick_y) {
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

esp_err_t MoveL298nDistance(bool forward, int distance_cm, const char* owner) {
    return L298nController::Instance().MoveDistanceFromCommand(forward, distance_cm, owner);
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
