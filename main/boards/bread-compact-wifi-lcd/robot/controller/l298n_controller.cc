#include "controller_internal.h"

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <numbers>

#include "../../config.h"
#include "../sensors/hcsr04_sensor.h"
#include "../sensors/mpu6050_imu.h"

#if HEADING_PD_DEBUG || ODOMETRY_DEBUG
namespace {

void ControlMonitorTask(void*) {
#if HEADING_PD_DEBUG
    bool fault_reported = false;
#endif
#if ODOMETRY_DEBUG
    ESP_LOGI(
        "Odometry",
        "time_ms,mode,state,valid,x_m,y_m,theta_rad,left_m,right_m,"
        "wheel_heading_valid,wheel_theta_rad,target_m,progress_m,error_m,position_error_m,fault");
#endif
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(200));
        const auto status = GetL298nMotorStatus();
#if ODOMETRY_DEBUG
        const auto& pose = status.odometry;
        const auto& ekf = status.ekf;
        const auto& drive = status.command.distance_drive;
        ESP_LOGI(
            "Odometry", "%lld,%s,%s,%d,%.4f,%.4f,%.5f,%.4f,%.4f,%d,%.5f,%.3f,%.4f,%.4f,%.4f,%s",
            static_cast<long long>(esp_timer_get_time() / 1000), status.command.mode, drive.state,
            pose.valid, pose.x_m, pose.y_m, pose.theta_rad, pose.left_m, pose.right_m,
            pose.wheel_heading_valid, pose.wheel_theta_rad, drive.target_m, drive.progress_m,
            drive.error_m, drive.position_error_m, pose.valid ? drive.fault : pose.fault);
        ESP_LOGI("PoseEKF", "%lld,%d,%.4f,%.4f,%.5f,%.4f,%.4f,%.5f,%s",
                 static_cast<long long>(esp_timer_get_time() / 1000), ekf.valid, ekf.x_m,
                 ekf.y_m, ekf.theta_rad, std::sqrt(ekf.variance_x_m2),
                 std::sqrt(ekf.variance_y_m2), std::sqrt(ekf.variance_theta_rad2), ekf.fault);
#endif
#if HEADING_PD_DEBUG
        if (!status.heading_hold_active) {
            if (!fault_reported && (std::strcmp(status.heading_fault, "none") != 0 ||
                                    std::strcmp(status.speed_pi_fault, "none") != 0)) {
                ESP_LOGW("HeadingPD", "control stopped: heading=%s, speed_pi=%s",
                         status.heading_fault, status.speed_pi_fault);
                fault_reported = true;
            }
            continue;
        }
        fault_reported = false;
        ESP_LOGI("HeadingPD",
                 "limit=%d%%, ref=%.2f deg, actual=%.2f deg, error=%.2f deg, "
                 "gyro=%.2f deg/s, correction=%.2f RPM, "
                 "left=%.1f/%.1f RPM PWM=%d, right=%.1f/%.1f RPM PWM=%d",
                 status.active_speed_percent, status.heading_target_degrees, status.heading_degrees,
                 status.heading_error_degrees, status.gyro_z_dps, status.heading_correction_rpm,
                 status.speed_pi_left_target_rpm, status.encoder.left.rpm, status.left_pwm_percent,
                 status.speed_pi_right_target_rpm, status.encoder.right.rpm,
                 status.right_pwm_percent);
#endif
    }
}

}  // namespace
#endif

namespace ruby_robot {

L298nController& L298nController::Instance() {
    static L298nController controller;
    return controller;
}

void L298nController::Stop() {
    std::lock_guard lock(mutex_);
    EndWebControlLocked();  // A spoken stop also invalidates in-flight browser commands.
}

esp_err_t L298nController::StartCommand(const CommandRequest& request) {
    static_assert(HEADING_TURN_AUTO_SPEED_PERCENT >= MotionGoalCommand::kMinTurnSpeedPercent &&
                  HEADING_TURN_AUTO_SPEED_PERCENT <= 100);
    auto resolved_request = request;
    if (auto* turn = std::get_if<AngleTurnRequest>(&resolved_request);
        turn && turn->speed_percent == 0)
        turn->speed_percent = HEADING_TURN_AUTO_SPEED_PERCENT;
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

L298nMotorStatus L298nController::Snapshot() {
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
                                          SPEED_PI_LEFT_REFERENCE_RPM /
                                          SPEED_PI_REFERENCE_PWM_PERCENT;
        status.speed_pi_right_target_rpm = std::abs(requested_right_percent_) *
                                           SPEED_PI_RIGHT_REFERENCE_RPM /
                                           SPEED_PI_REFERENCE_PWM_PERCENT;
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
    status.control_owner = web_control_.active() ? "web" : status.command.owner;
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
#if HEADING_PD_DEBUG || ODOMETRY_DEBUG
    if (xTaskCreate(ControlMonitorTask, "control_monitor", 4096, nullptr, 2, nullptr) != pdPASS)
        ESP_LOGW("MotorControl", "Could not start control monitor");
#endif
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

esp_err_t ReleaseL298nWebControl(const L298nWebDriveRequest& request) {
    return L298nController::Instance().ReleaseWebControl(request);
}

void SetL298nWebConnected(bool connected) {
    L298nController::Instance().SetWebConnected(connected);
}

esp_err_t StartL298nCommand(const ruby_robot::CommandRequest& request) {
    return L298nController::Instance().StartCommand(request);
}

L298nMotorStatus GetL298nMotorStatus() { return L298nController::Instance().Snapshot(); }

namespace {

esp_err_t StartLinearMotion(const L298nLinearRequest& request, ruby_robot::Motion motion) {
    if (const auto* web = std::get_if<L298nWebDriveRequest>(&request))
        return L298nController::Instance().WebCommand(*web, motion);
    auto goal = std::get<ruby_robot::DistanceDriveRequest>(request);
    if (goal.distance_cm < ruby_robot::MotionGoalCommand::kMinDistanceCm ||
        goal.distance_cm > ruby_robot::MotionGoalCommand::kMaxDistanceCm)
        return ESP_ERR_INVALID_ARG;
    if (motion == ruby_robot::Motion::kBackward)
        goal.distance_cm = -goal.distance_cm;
    if (goal.speed_percent == 0)
        goal.speed_percent = DISTANCE_DRIVE_MAX_SPEED_PERCENT;
    return StartL298nCommand(goal);
}

esp_err_t StartTurnMotion(const L298nTurnRequest& request, ruby_robot::Motion motion) {
    if (const auto* web = std::get_if<L298nWebDriveRequest>(&request))
        return L298nController::Instance().WebCommand(*web, motion);
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

esp_err_t ResetL298nOdometry() { return L298nController::Instance().ResetOdometry(); }

esp_err_t SetL298nFollowMode(bool) { return ESP_ERR_NOT_SUPPORTED; }

esp_err_t SetL298nObstacleAvoidance(bool) { return ESP_ERR_NOT_SUPPORTED; }

esp_err_t SetL298nFollowDistance(int) { return ESP_ERR_NOT_SUPPORTED; }

esp_err_t SetL298nObstacleDistance(int) { return ESP_ERR_NOT_SUPPORTED; }

esp_err_t StopL298nMotor(const L298nWebDriveRequest* request) {
    if (request != nullptr)
        return L298nController::Instance().WebCommand(*request, ruby_robot::Motion::kStop);
    L298nController::Instance().Stop();
    return ESP_OK;
}
