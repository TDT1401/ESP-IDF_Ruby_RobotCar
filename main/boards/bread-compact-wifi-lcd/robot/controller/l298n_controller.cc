#include "controller_internal.h"

#include <esp_system.h>

#include <cstdlib>

#include "../../config.h"
#include "../sensors/hcsr04_sensor.h"
#include "../sensors/mpu6050_imu.h"

namespace ruby_robot {

L298nController& L298nController::Instance() {
    static L298nController controller;
    return controller;
}

void L298nController::Stop() {
    std::lock_guard lock(mutex_);
    web_control_.Revoke();  // A spoken stop also invalidates in-flight browser commands.
    ReleaseWebSleepLockLocked();
    StopLocked();
}

L298nMotorStatus L298nController::Snapshot() {
    std::lock_guard lock(mutex_);
    const bool distance_fresh =
        sensor_valid_ && esp_timer_get_time() - last_sensor_update_us_ <= kSensorStaleMs * 1000LL;
    const auto imu = GetMpu6050Status();
    L298nMotorStatus status{};
    status.motion = MotionName(motion_);
    status.speed_percent = web_speed_percent_;
    status.active_speed_percent = moving_ ? ActiveSpeedLocked() : 0;
    status.moving = moving_;
    status.pending = motion_ != Motion::kStop && !moving_;
    status.web_manual = web_control_.active();
    status.autonomy_mode = "manual";
    status.follow_distance_cm = kDefaultFollowDistanceCm;
    status.obstacle_distance_cm = kDefaultObstacleDistanceCm;
    status.follow_target_effective_cm = kDefaultFollowDistanceCm;
    status.obstacle_stop_effective_cm = kDefaultObstacleDistanceCm;
    status.web_warning_effective_cm =
        ScaleDistanceForSpeed(kDefaultObstacleDistanceCm, web_speed_percent_);
    status.measured_distance_cm = distance_fresh ? measured_distance_cm_ : -1;
    status.distance_valid = distance_fresh;
    status.hardware_error = fault_;
    status.encoder = encoder_.Snapshot();
    status.speed_pi_calibrated = SpeedPiReadyLocked();
    status.speed_pi_active = moving_ && status.speed_pi_calibrated &&
                             (requested_left_percent_ != 0 || requested_right_percent_ != 0);
    if (SpeedPiConfiguredLocked()) {
        status.speed_pi_left_target_rpm = std::abs(requested_left_percent_) *
                                          SPEED_PI_LEFT_REFERENCE_RPM /
                                          SPEED_PI_REFERENCE_PWM_PERCENT;
        status.speed_pi_right_target_rpm = std::abs(requested_right_percent_) *
                                           SPEED_PI_RIGHT_REFERENCE_RPM /
                                           SPEED_PI_REFERENCE_PWM_PERCENT;
    }
    status.speed_pi_target_rpm =
        (status.speed_pi_left_target_rpm + status.speed_pi_right_target_rpm) / 2.0;
    status.speed_pi_fault = speed_pi_fault_;
    status.distance_move_state = "disabled";
    status.imu_ready = imu.ready;
    status.imu_error = imu.error;
    status.gyro_z_dps = imu.gyro_z_dps;
    status.left_pwm_percent = CurrentLeftPwmPercentLocked();
    status.right_pwm_percent = CurrentRightPwmPercentLocked();
    status.control_owner = web_control_.active() ? "web" : "none";
    status.warning_sound_enabled = proximity_warning_enabled_;
    return status;
}

L298nController::L298nController() {
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
    // Singleton and sensor callbacks live for the entire firmware lifetime.
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
    (void)speed_percent;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t MoveL298nDistance(bool forward, int distance_cm, const char* owner) {
    (void)forward;
    (void)distance_cm;
    (void)owner;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t SetL298nFollowMode(bool enabled) {
    (void)enabled;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t SetL298nObstacleAvoidance(bool enabled) {
    (void)enabled;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t SetL298nFollowDistance(int distance_cm) {
    (void)distance_cm;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t SetL298nObstacleDistance(int distance_cm) {
    (void)distance_cm;
    return ESP_ERR_NOT_SUPPORTED;
}

void StopL298nMotor() { L298nController::Instance().Stop(); }
