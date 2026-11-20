#include "controller_internal.h"

#include <esp_log.h>
#include <esp_system.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <numbers>

#include "imu.h"
#include "ultrasonic_sensor.h"

namespace ruby_robot {
namespace {

MotorController* controller_instance = nullptr;

}  // namespace

void MotorController::Initialize(const MotorControllerConfig& config) {
    static MotorController controller(config);
    controller_instance = &controller;
}

MotorController& MotorController::Instance() {
    assert(controller_instance != nullptr);
    return *controller_instance;
}

void MotorController::Stop() {
    std::lock_guard lock(mutex_);
    EndWebControlLocked();  // A spoken stop also invalidates in-flight browser commands.
}

esp_err_t MotorController::StartCommand(const CommandRequest& request) {
    auto resolved_request = request;
    if (auto* turn = std::get_if<AngleTurnRequest>(&resolved_request);
        turn && turn->speed_percent == 0)
        turn->speed_percent = config_.heading_turn_auto_speed_percent;
    if (!CommandInterface::Valid(resolved_request))
        return ESP_ERR_INVALID_ARG;
    const auto requirements = CommandInterface::Requirements(resolved_request);
    std::lock_guard lock(mutex_);
    if (!MotionAllowed() || (requirements.wifi && !web_control_.connected()) ||
        web_control_.active() || command_interface_.mode() != CommandMode::kIdle)
        return ESP_ERR_INVALID_STATE;
    if (fault_ != ESP_OK)
        return fault_;
    if (requirements.speed_pi && !SpeedPiReadyLocked())
        return ESP_ERR_INVALID_STATE;
    const int64_t feedback_us = esp_timer_get_time();
    UpdateOdometryLocked(feedback_us);
    if (requirements.odometry &&
        (!odometry_.Snapshot().valid || !OdometryStationaryLocked(feedback_us)))
        return ESP_ERR_INVALID_STATE;
    Mpu6050Status imu;
    if (requirements.heading) {
        imu = GetMpu6050Status();
        if (!imu.ready || !imu.heading_valid) {
            heading_fault_ = !imu.ready ? "imu_unavailable" : "heading_invalid";
            return ESP_ERR_INVALID_STATE;
        }
    }
    StopLocked();
    if (fault_ != ESP_OK)
        return fault_;
    speed_pi_fault_ = "none";
    heading_fault_ = "none";
    const int64_t now = esp_timer_get_time();
    const auto goal_pose = std::holds_alternative<NavigateRequest>(resolved_request)
                               ? NavigationPoseLocked()
                               : odometry_.Snapshot();
    if (!command_interface_.Start(resolved_request, now, goal_pose, DistanceLimits()))
        return ESP_ERR_INVALID_STATE;
    if (!PrepareMotionLocked(command_interface_.direction())) {
        FinishCommandLocked(esp_timer_get_time(), false, "motor_error");
        return fault_ != ESP_OK ? fault_ : ESP_FAIL;
    }
    if (requirements.heading) {
        heading_hold_active_ = true;
        heading_turn_active_ = command_interface_.IsAngleTurn() ||
                               command_interface_.IsNavigationTurn();
        heading_turn_start_degrees_ = imu.heading_degrees;
        turn_progress_degrees_ = 0;
        const int angle = command_interface_.angle_degrees();
        const int sign = command_interface_.direction() == Motion::kLeft ? 1 : -1;
        heading_target_degrees_ = imu.heading_degrees +
                                  (command_interface_.IsNavigation()
                                       ? command_interface_.NavigationHeadingErrorRad() * 180.0 /
                                             std::numbers::pi
                                       : sign * angle);
    }
    return ESP_OK;
}

L298nMotorStatus MotorController::Snapshot() {
    std::lock_guard lock(mutex_);
    const bool distance_fresh =
        sensor_valid_ && esp_timer_get_time() - last_sensor_update_us_ <= kSensorStaleMs * 1000LL;
    const auto imu = GetMpu6050Status();
    L298nMotorStatus status{};
    const int64_t now = esp_timer_get_time();
    status.command = command_interface_.Snapshot(now);
    if (command_interface_.mode() != CommandMode::kManual &&
        command_interface_.last_mode() == CommandMode::kAngleTurn) {
        status.turn_target_degrees = std::abs(status.command.angle_turn.angle_degrees);
        status.turn_progress_degrees = turn_progress_degrees_;
        status.turn_uses_imu = status.turn_target_degrees != 0;
    }
    status.motion = MotionName(motion_);
    status.speed_percent = web_speed_percent_;
    status.active_speed_percent = moving_ ? ActiveSpeedLocked() : 0;
    status.moving = moving_;
    status.pending = motion_ != Motion::kStop && !moving_;
    status.web_manual = web_control_.active();
    status.obstacle_distance_cm = kDefaultObstacleDistanceCm;
    status.obstacle_stop_effective_cm = kDefaultObstacleDistanceCm;
    status.web_warning_effective_cm =
        ScaleDistanceForSpeed(kDefaultObstacleDistanceCm, web_speed_percent_);
    status.measured_distance_cm = distance_fresh ? measured_distance_cm_ : -1;
    status.distance_valid = distance_fresh;
    status.hardware_error = fault_;
    status.encoder = encoder_.Snapshot();
    status.odometry = odometry_.Snapshot();
    status.ekf = ekf_.Snapshot();
    if (status.odometry.valid && now - status.odometry.sampled_at_us > Odometry::kMaximumGapUs) {
        status.odometry.valid = status.odometry.wheel_heading_valid = false;
        status.odometry.fault = "sample_gap";
    }
    if (status.ekf.valid && now - status.ekf.sampled_at_us > Odometry::kMaximumGapUs) {
        status.ekf.valid = false;
        status.ekf.fault = "sample_gap";
    }
    status.speed_pi_calibrated = SpeedPiReadyLocked();
    status.speed_pi_active = moving_ && status.speed_pi_calibrated &&
                             (requested_left_percent_ != 0 || requested_right_percent_ != 0);
    if (SpeedPiConfiguredLocked()) {
        status.speed_pi_left_target_rpm = std::abs(requested_left_percent_) *
                                          config_.speed_pi_left_reference_rpm /
                                          config_.speed_pi_reference_pwm_percent;
        status.speed_pi_right_target_rpm = std::abs(requested_right_percent_) *
                                           config_.speed_pi_right_reference_rpm /
                                           config_.speed_pi_reference_pwm_percent;
    }
    status.speed_pi_target_rpm =
        (status.speed_pi_left_target_rpm + status.speed_pi_right_target_rpm) / 2.0;
    status.speed_pi_fault = speed_pi_fault_;
    const auto& distance = status.command.distance_drive;
    status.distance_move_state = distance.state;
    status.distance_move_target_cm =
        static_cast<int>(std::lround(std::abs(distance.target_m) * 100));
    status.distance_move_progress_cm =
        distance.progress_m * (distance.target_m < 0 ? -100.0 : 100.0);
    status.imu_ready = imu.ready;
    status.imu_error = imu.error;
    status.heading_valid = imu.heading_valid;
    status.heading_degrees = imu.heading_degrees;
    status.gyro_z_dps = imu.gyro_z_dps;
    status.heading_hold_active = heading_hold_active_;
    status.heading_target_degrees = heading_target_degrees_;
    status.heading_error_degrees = heading_error_degrees_;
    status.heading_correction_rpm = heading_correction_rpm_;
    status.heading_fault = heading_fault_;
    status.left_pwm_percent = moving_ ? applied_left_pwm_percent_ : 0;
    status.right_pwm_percent = moving_ ? applied_right_pwm_percent_ : 0;
    status.rear_left_pwm_percent = status.left_pwm_percent;
    status.rear_right_pwm_percent = status.right_pwm_percent;
    status.control_owner = web_control_.active() ? "web" : status.command.owner;
    status.warning_sound_enabled = proximity_warning_enabled_;
    return status;
}

MotorController::MotorController(const MotorControllerConfig& config)
    : config_(config), driver_(config_.motor), encoder_(config_.encoder) {
    assert(config_.motion_allowed != nullptr && config_.schedule != nullptr &&
           config_.play_proximity_warning != nullptr);
#ifdef CONFIG_PM_ENABLE
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "l298n", &sleep_lock_));
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "l298n_web", &web_sleep_lock_));
#endif
    encoder_.Initialize();
    InitializeMpu6050(config_.imu);
    esp_timer_create_args_t timer = {};
    timer.callback = [](void* arg) { static_cast<MotorController*>(arg)->Tick(); };
    timer.arg = this;
    timer.dispatch_method = ESP_TIMER_TASK;
    timer.name = "l298n_deadline";
    timer.skip_unhandled_events = true;
    ESP_ERROR_CHECK(esp_timer_create(&timer, &timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_, kTickUs));
}

MotorController::~MotorController() {
    esp_timer_stop(timer_);
    esp_timer_delete(timer_);
    Stop();
#ifdef CONFIG_PM_ENABLE
    esp_pm_lock_delete(sleep_lock_);
    esp_pm_lock_delete(web_sleep_lock_);
#endif
}

bool MotorController::Check(esp_err_t error) {
    if (error != ESP_OK && fault_ == ESP_OK) {
        fault_ = error;  // Latch hardware errors until reboot; never auto-resume.
    }
    return error == ESP_OK;
}

}  // namespace ruby_robot

using ruby_robot::MotorController;

void InitializeL298nController(const ruby_robot::MotorControllerConfig& config) {
    // Singleton and sensor callbacks live for the entire firmware lifetime.
    MotorController::Initialize(config);
    InitializeHcsr04Sensor(config.ultrasonic, [](int distance_cm, bool valid) {
        auto& controller = MotorController::Instance();
        controller.UpdateDistance(distance_cm, valid);
        controller.ScheduleProximityWarning();
    });
    ESP_ERROR_CHECK(esp_register_shutdown_handler([]() { MotorController::Instance().Stop(); }));
}

esp_err_t BeginL298nWebControl(uint32_t* session) {
    return MotorController::Instance().BeginWebControl(session);
}

esp_err_t ReleaseL298nWebControl(const L298nWebDriveRequest& request) {
    return MotorController::Instance().ReleaseWebControl(request);
}

void SetL298nWebConnected(bool connected) {
    MotorController::Instance().SetWebConnected(connected);
}

esp_err_t StartL298nCommand(const ruby_robot::CommandRequest& request) {
    return MotorController::Instance().StartCommand(request);
}

L298nMotorStatus GetL298nMotorStatus() { return MotorController::Instance().Snapshot(); }

namespace {

esp_err_t StartLinearMotion(const L298nLinearRequest& request, ruby_robot::Motion motion) {
    if (const auto* web = std::get_if<L298nWebDriveRequest>(&request))
        return MotorController::Instance().WebCommand(*web, motion);
    auto goal = std::get<ruby_robot::DistanceDriveRequest>(request);
    if (goal.distance_cm < ruby_robot::MotionGoalCommand::kMinDistanceCm ||
        goal.distance_cm > ruby_robot::MotionGoalCommand::kMaxDistanceCm)
        return ESP_ERR_INVALID_ARG;
    if (motion == ruby_robot::Motion::kBackward)
        goal.distance_cm = -goal.distance_cm;
    if (goal.speed_percent == 0)
        goal.speed_percent = MotorController::Instance().DefaultDistanceSpeed();
    return StartL298nCommand(goal);
}

esp_err_t StartTurnMotion(const L298nTurnRequest& request, ruby_robot::Motion motion) {
    if (const auto* web = std::get_if<L298nWebDriveRequest>(&request))
        return MotorController::Instance().WebCommand(*web, motion);
    auto goal = std::get<ruby_robot::AngleTurnRequest>(request);
    if (goal.angle_degrees < ruby_robot::MotionGoalCommand::kMinAngleDegrees ||
        goal.angle_degrees > ruby_robot::MotionGoalCommand::kMaxAngleDegrees)
        return ESP_ERR_INVALID_ARG;
    if (motion == ruby_robot::Motion::kRight)
        goal.angle_degrees = -goal.angle_degrees;
    return StartL298nCommand(goal);
}

}  // namespace

esp_err_t MoveL298nForward(const L298nLinearRequest& request) {
    return StartLinearMotion(request, ruby_robot::Motion::kForward);
}

esp_err_t MoveL298nBackward(const L298nLinearRequest& request) {
    return StartLinearMotion(request, ruby_robot::Motion::kBackward);
}

esp_err_t TurnL298nLeft(const L298nTurnRequest& request) {
    return StartTurnMotion(request, ruby_robot::Motion::kLeft);
}

esp_err_t TurnL298nRight(const L298nTurnRequest& request) {
    return StartTurnMotion(request, ruby_robot::Motion::kRight);
}

esp_err_t ResetL298nOdometry() { return MotorController::Instance().ResetOdometry(); }

esp_err_t StopL298nMotor(const L298nWebDriveRequest* request) {
    if (request != nullptr)
        return MotorController::Instance().WebCommand(*request, ruby_robot::Motion::kStop);
    MotorController::Instance().Stop();
    return ESP_OK;
}
