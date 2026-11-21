#include "controller_internal.h"

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/task.h>

#include <cassert>
#include <cmath>
#include <cstdlib>

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
    const auto* turn = std::get_if<AngleTurnRequest>(&request);
    const auto* drive = std::get_if<DistanceDriveRequest>(&request);
    if ((!turn && !drive) || (turn && !ValidAngleTurn(*turn)) ||
        (drive && !DistanceDrive::Valid(*drive)))
        return ESP_ERR_INVALID_ARG;
    const bool needs_odometry = turn == nullptr;
    std::lock_guard lock(mutex_);
    if (!MotionAllowed() || !web_control_.connected() || web_control_.active() ||
        mode_ != CommandMode::kIdle)
        return ESP_ERR_INVALID_STATE;
    if (fault_ != ESP_OK)
        return fault_;
    if (!SpeedPiReadyLocked())
        return ESP_ERR_INVALID_STATE;
    const int64_t feedback_us = esp_timer_get_time();
    UpdateOdometryLocked(feedback_us);
    if (needs_odometry && (!odometry_.Snapshot().valid || !OdometryStationaryLocked(feedback_us)))
        return ESP_ERR_INVALID_STATE;
    const auto imu = GetMpu6050Status();
    if (!imu.ready || !imu.heading_valid) {
        heading_fault_ = !imu.ready ? "imu_unavailable" : "heading_invalid";
        return ESP_ERR_INVALID_STATE;
    }
    StopLocked();
    if (fault_ != ESP_OK)
        return fault_;
    speed_pi_fault_ = "none";
    heading_fault_ = "none";
    const int64_t now = esp_timer_get_time();
    if (!StartGoalLocked(request, now, odometry_.Snapshot(), DistanceLimits()))
        return ESP_ERR_INVALID_STATE;
    if (!PrepareMotionLocked(GoalDirectionLocked())) {
        FinishCommandLocked(esp_timer_get_time(), false, "motor_error");
        return fault_ != ESP_OK ? fault_ : ESP_FAIL;
    }
    heading_hold_active_ = true;
    heading_turn_active_ = IsAngleTurnLocked();
    heading_turn_start_degrees_ = imu.heading_degrees;
    turn_progress_degrees_ = 0;
    const int angle = GoalAngleDegreesLocked();
    const int sign = GoalDirectionLocked() == Motion::kLeft ? 1 : -1;
    heading_target_degrees_ = imu.heading_degrees + sign * angle;
    return ESP_OK;
}

L298nMotorStatus MotorController::Snapshot() {
    std::lock_guard lock(mutex_);
    const bool distance_fresh =
        sensor_valid_ && esp_timer_get_time() - last_sensor_update_us_ <= kSensorStaleMs * 1000LL;
    const auto imu = GetMpu6050Status();
    L298nMotorStatus status{};
    const int64_t now = esp_timer_get_time();
    status.command = CommandSnapshotLocked(now);
    if (mode_ != CommandMode::kManual && last_mode_ == CommandMode::kAngleTurn) {
        status.turn_target_degrees = std::abs(status.command.angle_turn.angle_degrees);
        status.turn_progress_degrees = turn_progress_degrees_;
        status.turn_uses_imu = status.turn_target_degrees != 0;
    }
    status.motion = MotionName(motion_);
    status.speed_percent = web_speed_percent_;
    status.active_speed_percent = moving_ ? ActiveSpeedLocked() : 0;
    if (IsAngleTurnLocked())
        status.command.angle_turn.speed_percent = status.active_speed_percent;
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
    task_stopped_ = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(task_stopped_ != nullptr ? ESP_OK : ESP_ERR_NO_MEM);
    const auto result = xTaskCreate(
        [](void* arg) {
            static_cast<MotorController*>(arg)->RunControlTask();
            vTaskDelete(nullptr);
        },
        "motor_control", 4096, this, 5, nullptr);
    if (result != pdPASS) {
        vSemaphoreDelete(task_stopped_);
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
}

MotorController::~MotorController() {
    // Let the task finish Tick() and release its mutex before destroying shared state.
    stop_task_.store(true);
    xSemaphoreTake(task_stopped_, portMAX_DELAY);
    vSemaphoreDelete(task_stopped_);
    Stop();
#ifdef CONFIG_PM_ENABLE
    esp_pm_lock_delete(sleep_lock_);
    esp_pm_lock_delete(web_sleep_lock_);
#endif
}

void MotorController::RunControlTask() {
    constexpr TickType_t period = pdMS_TO_TICKS(kControlPeriodMs);
    static_assert(period > 0, "FreeRTOS tick rate must support the motor control period");
    TickType_t last_wake = xTaskGetTickCount();
    while (!stop_task_.load()) {
        vTaskDelayUntil(&last_wake, period);
        if (stop_task_.load())
            break;
        Tick();
        const TickType_t now = xTaskGetTickCount();
        // Skip missed periods instead of running a burst of overdue control updates.
        if (static_cast<TickType_t>(now - last_wake) >= period)
            last_wake = now;
    }
    xSemaphoreGive(task_stopped_);
}

bool MotorController::Check(esp_err_t error) {
    if (error != ESP_OK && fault_ == ESP_OK) {
        fault_ = error;  // Latch hardware errors until reboot; never auto-resume.
    }
    return error == ESP_OK;
}

bool MotorController::StartGoalLocked(const CommandRequest& request, int64_t now_us,
                                     const OdometrySnapshot& pose,
                                     const DistanceDriveLimits& limits) {
    if (mode_ != CommandMode::kIdle)
        return false;
    if (const auto* turn = std::get_if<AngleTurnRequest>(&request)) {
        turn_request_ = *turn;
        turn_state_ = AngleTurnState::kRunning;
        turn_deadline_us_ = now_us + kTurnTimeoutUs;
        last_mode_ = mode_ = CommandMode::kAngleTurn;
        return true;
    }
    if (const auto* drive = std::get_if<DistanceDriveRequest>(&request)) {
        if (!distance_drive_.Start(*drive, limits, pose, now_us))
            return false;
        last_mode_ = mode_ = CommandMode::kDistanceDrive;
        return true;
    }
    return false;
}

Motion MotorController::GoalDirectionLocked() const {
    if (mode_ == CommandMode::kDistanceDrive)
        return distance_drive_.direction();
    if (mode_ == CommandMode::kAngleTurn)
        return turn_request_.angle_degrees > 0   ? Motion::kLeft
               : turn_request_.angle_degrees < 0 ? Motion::kRight
                                                 : Motion::kStop;
    return Motion::kStop;
}

int MotorController::GoalSpeedLocked() const {
    if (mode_ == CommandMode::kDistanceDrive)
        return distance_drive_.speed_percent();
    return 0;  // The controller computes angle-turn speed from IMU feedback.
}

int MotorController::GoalAngleDegreesLocked() const {
    return mode_ == CommandMode::kAngleTurn ? std::abs(turn_request_.angle_degrees) : 0;
}

bool MotorController::GoalExpiredLocked(int64_t now_us) const {
    if (mode_ == CommandMode::kDistanceDrive)
        return distance_drive_.Expired(now_us);
    return mode_ == CommandMode::kAngleTurn && now_us >= turn_deadline_us_;
}

bool MotorController::GoalCompletionReadyLocked() const {
    if (mode_ == CommandMode::kDistanceDrive)
        return distance_drive_.CompletionReady();
    return false;
}

void MotorController::CompleteGoalLocked(int64_t now_us) {
    if (mode_ == CommandMode::kAngleTurn)
        turn_state_ = AngleTurnState::kCompleted;
    else if (mode_ == CommandMode::kDistanceDrive)
        distance_drive_.Finish(DistanceDriveState::kCompleted, now_us);
    mode_ = CommandMode::kIdle;
}

void MotorController::FailGoalLocked(int64_t now_us, const char* fault) {
    if (mode_ == CommandMode::kAngleTurn)
        turn_state_ = AngleTurnState::kFault;
    else if (mode_ == CommandMode::kDistanceDrive)
        distance_drive_.Finish(DistanceDriveState::kFault, now_us, fault);
    mode_ = CommandMode::kIdle;
}

bool MotorController::UpdateGoalLocked(const OdometrySnapshot& pose, double left_rpm,
                                      double right_rpm, int64_t now_us) {
    return mode_ != CommandMode::kDistanceDrive ||
           distance_drive_.Update(pose, left_rpm, right_rpm, now_us);
}

const char* MotorController::GoalFaultLocked() const {
    return mode_ == CommandMode::kDistanceDrive ? distance_drive_.fault() : "none";
}

void MotorController::StopGoalLocked(int64_t now_us) {
    if (mode_ == CommandMode::kAngleTurn) {
        mode_ = CommandMode::kIdle;
        turn_state_ = AngleTurnState::kInterrupted;
    } else if (mode_ == CommandMode::kDistanceDrive) {
        distance_drive_.Finish(DistanceDriveState::kInterrupted, now_us);
        mode_ = CommandMode::kIdle;
    }
}

CommandSnapshot MotorController::CommandSnapshotLocked(int64_t now_us) const {
    const auto distance = distance_drive_.Snapshot(now_us);
    const auto turn = TurnSnapshotLocked();
    const char* owner = mode_ == CommandMode::kDistanceDrive ? distance.owner
                        : mode_ == CommandMode::kAngleTurn   ? turn.owner
                        : mode_ == CommandMode::kManual      ? "web"
                                                             : "none";
    return {ModeName(mode_), owner, turn, distance};
}

const char* MotorController::ModeName(CommandMode mode) {
    switch (mode) {
        case CommandMode::kManual:
            return "manual";
        case CommandMode::kAngleTurn:
            return "angle_turn";
        case CommandMode::kDistanceDrive:
            return "distance_drive";
        default:
            return "idle";
    }
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

esp_err_t StartL298nCommand(const ruby_robot::CommandRequest& request) {
    return MotorController::Instance().StartCommand(request);
}

L298nMotorStatus GetL298nMotorStatus() { return MotorController::Instance().Snapshot(); }

namespace {

esp_err_t StartLinearMotion(const L298nLinearRequest& request, ruby_robot::Motion motion) {
    if (const auto* web = std::get_if<L298nWebDriveRequest>(&request))
        return ruby_robot::MotorController::Instance().WebCommand(*web, motion);
    auto goal = std::get<ruby_robot::DistanceDriveRequest>(request);
    if (goal.distance_cm < ruby_robot::DistanceDrive::kMinDistanceCm ||
        goal.distance_cm > ruby_robot::DistanceDrive::kMaxDistanceCm)
        return ESP_ERR_INVALID_ARG;
    if (motion == ruby_robot::Motion::kBackward)
        goal.distance_cm = -goal.distance_cm;
    return StartL298nCommand(goal);
}

}  // namespace

esp_err_t MoveL298nForward(const L298nLinearRequest& request) {
    return StartLinearMotion(request, ruby_robot::Motion::kForward);
}

esp_err_t MoveL298nBackward(const L298nLinearRequest& request) {
    return StartLinearMotion(request, ruby_robot::Motion::kBackward);
}


namespace {

esp_err_t StartTurnMotion(const L298nTurnRequest& request, ruby_robot::Motion motion) {
    if (const auto* web = std::get_if<L298nWebDriveRequest>(&request))
        return ruby_robot::MotorController::Instance().WebCommand(*web, motion);
    auto goal = std::get<ruby_robot::AngleTurnRequest>(request);
    if (goal.angle_degrees < ruby_robot::kMinAngleDegrees ||
        goal.angle_degrees > ruby_robot::kMaxAngleDegrees)
        return ESP_ERR_INVALID_ARG;
    if (motion == ruby_robot::Motion::kRight)
        goal.angle_degrees = -goal.angle_degrees;
    return StartL298nCommand(goal);
}

}  // namespace

esp_err_t TurnL298nLeft(const L298nTurnRequest& request) {
    return StartTurnMotion(request, ruby_robot::Motion::kLeft);
}

esp_err_t TurnL298nRight(const L298nTurnRequest& request) {
    return StartTurnMotion(request, ruby_robot::Motion::kRight);
}
