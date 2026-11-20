#include "robot_board.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

#include <cinttypes>
#include <cmath>
#include <cstring>
#include <utility>

#include "board_config.h"
#include "application.h"
#include "assets/lang_config.h"

#include "mcp/mcp_tools.h"
#include "motor_controller.h"
#include "serialization/fc03_json.h"
#include "serialization/odometry_json.h"
#include "telegram.h"
#include "telegram_config.local.h"
#include "web_server.h"
#include "wifi_manager.h"

namespace {

using ruby_robot::MotorDriverConfig;
using ruby_robot::Wheel;

static_assert(MPU6050_SDA_PIN != MPU6050_SCL_PIN);
static_assert(MPU6050_SDA_PIN != LAMP_GPIO && MPU6050_SCL_PIN != LAMP_GPIO);
static_assert(HCSR04_SERVO_CENTER_PULSE_US > 0 && HCSR04_SERVO_CENTER_PULSE_US < 20000);
static_assert(SPEED_PI_REFERENCE_PWM_PERCENT > 0 && SPEED_PI_REFERENCE_PWM_PERCENT <= 100);
static_assert(SPEED_PI_LEFT_REFERENCE_RPM >= 0 && SPEED_PI_RIGHT_REFERENCE_RPM >= 0);
static_assert(SPEED_PI_LEFT_KP >= 0 && SPEED_PI_LEFT_KI >= 0);
static_assert(SPEED_PI_RIGHT_KP >= 0 && SPEED_PI_RIGHT_KI >= 0);
static_assert(HEADING_PD_KP_RPM_PER_DEG >= 0 && HEADING_PD_KD_RPM_PER_DPS >= 0);
static_assert(HEADING_PD_MAX_CORRECTION_RPM >= 0);
static_assert(HEADING_TURN_MAX_RPM >= HEADING_TURN_MIN_RPM);
static_assert(HEADING_TURN_KP_RPM_PER_DEG > 0);
static_assert(HEADING_TURN_ACCELERATION_RPM_PER_S > 0 && HEADING_TURN_DECELERATION_RPM_PER_S > 0);
static_assert(HEADING_TURN_TOLERANCE_DEGREES > 0);
static_assert(HEADING_TURN_SETTLE_RATE_DPS > 0);
static_assert(HEADING_TURN_SETTLE_RPM >= 0);
static_assert(HEADING_TURN_MIN_RPM > 0);
static_assert(HEADING_TURN_LEFT_FRICTION_PWM_PERCENT >= 0 &&
              HEADING_TURN_LEFT_FRICTION_PWM_PERCENT < 100);
static_assert(HEADING_TURN_RIGHT_FRICTION_PWM_PERCENT >= 0 &&
              HEADING_TURN_RIGHT_FRICTION_PWM_PERCENT < 100);
static_assert(DISTANCE_DRIVE_LEFT_FRICTION_PWM_PERCENT >= 0 &&
              DISTANCE_DRIVE_LEFT_FRICTION_PWM_PERCENT < 100);
static_assert(DISTANCE_DRIVE_RIGHT_FRICTION_PWM_PERCENT >= 0 &&
              DISTANCE_DRIVE_RIGHT_FRICTION_PWM_PERCENT < 100);
static_assert(ODOMETRY_TRACK_WIDTH_MM >= 0);
static_assert(NAVIGATION_USE_EKF == 0 || NAVIGATION_USE_EKF == 1);
static_assert(DISTANCE_DRIVE_ACCELERATION_MPS2 > 0 && DISTANCE_DRIVE_DECELERATION_MPS2 > 0);
static_assert(DISTANCE_DRIVE_MAX_SPEED_PERCENT >= DISTANCE_DRIVE_MIN_SPEED_PERCENT &&
              DISTANCE_DRIVE_MAX_SPEED_PERCENT <= 100);
static_assert(DISTANCE_DRIVE_MIN_SPEED_PERCENT > 0 && DISTANCE_DRIVE_MIN_SPEED_PERCENT <= 100);
static_assert(DISTANCE_DRIVE_TOLERANCE_MM > 0 &&
              DISTANCE_DRIVE_TOLERANCE_MM < ruby_robot::MotionGoalCommand::kMinDistanceCm * 10);
static_assert(DISTANCE_DRIVE_SETTLE_RPM >= 0 && DISTANCE_DRIVE_SETTLE_MS > 0);

static_assert(FC03_LEFT_GPIO != FC03_RIGHT_GPIO);
static_assert(FC03_GLITCH_FILTER_NS > 0 && FC03_GLITCH_FILTER_NS <= 12000);
static_assert(FC03_COAST_MS >= 0 && FC03_COAST_MS <= 2000);
static_assert(FC03_LEFT_PULSES_PER_REV >= 0 && FC03_LEFT_PULSES_PER_REV <= 1000000 &&
              FC03_RIGHT_PULSES_PER_REV >= 0 && FC03_RIGHT_PULSES_PER_REV <= 1000000);
static_assert(FC03_LEFT_WHEEL_DIAMETER_MM >= 0 && FC03_LEFT_WHEEL_DIAMETER_MM <= 1000 &&
              FC03_RIGHT_WHEEL_DIAMETER_MM >= 0 && FC03_RIGHT_WHEEL_DIAMETER_MM <= 1000);

MotorDriverConfig MakeMotorDriverConfig() {
    MotorDriverConfig config = {};
    // Keep motor channels 1/2/4/5; channel 3 belongs to the servo.
    config.motors = {{
        {MOTOR_LEFT_EN_PIN, MOTOR_LEFT_IN1_PIN, MOTOR_LEFT_IN2_PIN, LEDC_CHANNEL_1, false},
        {MOTOR_RIGHT_EN_PIN, MOTOR_RIGHT_IN1_PIN, MOTOR_RIGHT_IN2_PIN, LEDC_CHANNEL_2, false},
        {MOTOR_REAR_LEFT_EN_PIN, MOTOR_REAR_LEFT_IN1_PIN, MOTOR_REAR_LEFT_IN2_PIN, LEDC_CHANNEL_4,
         false},
        {MOTOR_REAR_RIGHT_EN_PIN, MOTOR_REAR_RIGHT_IN1_PIN, MOTOR_REAR_RIGHT_IN2_PIN, LEDC_CHANNEL_5,
         false},
    }};
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_LEFT
    config.motors[static_cast<unsigned>(Wheel::kLeft)].inverted = true;
#endif
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_RIGHT
    config.motors[static_cast<unsigned>(Wheel::kRight)].inverted = true;
#endif
    config.mode = LEDC_LOW_SPEED_MODE;
    config.timer = LEDC_TIMER_1;
    config.duty_resolution = LEDC_TIMER_10_BIT;
    config.freq_hz = 1000;
    return config;
}

ruby_robot::MotorControllerConfig MakeMotorControllerConfig() {
    ruby_robot::MotorControllerConfig config = {};
    config.motor = MakeMotorDriverConfig();
    config.encoder.enabled = true;
    config.encoder.gpio = {FC03_LEFT_GPIO, FC03_RIGHT_GPIO};
    config.encoder.pulses_per_rev = {FC03_LEFT_PULSES_PER_REV, FC03_RIGHT_PULSES_PER_REV};
    config.encoder.wheel_diameter_mm = {FC03_LEFT_WHEEL_DIAMETER_MM, FC03_RIGHT_WHEEL_DIAMETER_MM};
    config.encoder.glitch_filter_ns = FC03_GLITCH_FILTER_NS;
    config.encoder.coast_ms = FC03_COAST_MS;
    config.imu = {MPU6050_SDA_PIN, MPU6050_SCL_PIN, MPU6050_I2C_ADDRESS, MPU6050_GYRO_Z_SIGN,
                  MPU6050_DEBUG != 0};
    config.ultrasonic = {HCSR04_TRIGGER_PIN, HCSR04_ECHO_PIN, HCSR04_SERVO_SIGNAL_PIN,
                         HCSR04_SERVO_CENTER_PULSE_US, LEDC_LOW_SPEED_MODE, LEDC_TIMER_2,
                         LEDC_CHANNEL_3};
    config.speed_pi_reference_pwm_percent = SPEED_PI_REFERENCE_PWM_PERCENT;
    config.speed_pi_left_reference_rpm = SPEED_PI_LEFT_REFERENCE_RPM;
    config.speed_pi_right_reference_rpm = SPEED_PI_RIGHT_REFERENCE_RPM;
    config.speed_pi_left_kp = SPEED_PI_LEFT_KP;
    config.speed_pi_left_ki = SPEED_PI_LEFT_KI;
    config.speed_pi_right_kp = SPEED_PI_RIGHT_KP;
    config.speed_pi_right_ki = SPEED_PI_RIGHT_KI;
    config.heading_pd_kp_rpm_per_deg = HEADING_PD_KP_RPM_PER_DEG;
    config.heading_pd_kd_rpm_per_dps = HEADING_PD_KD_RPM_PER_DPS;
    config.heading_pd_max_correction_rpm = HEADING_PD_MAX_CORRECTION_RPM;
    config.heading_turn_max_rpm = HEADING_TURN_MAX_RPM;
    config.heading_turn_kp_rpm_per_deg = HEADING_TURN_KP_RPM_PER_DEG;
    config.heading_turn_acceleration_rpm_per_s = HEADING_TURN_ACCELERATION_RPM_PER_S;
    config.heading_turn_deceleration_rpm_per_s = HEADING_TURN_DECELERATION_RPM_PER_S;
    config.heading_turn_min_rpm = HEADING_TURN_MIN_RPM;
    config.heading_turn_tolerance_degrees = HEADING_TURN_TOLERANCE_DEGREES;
    config.heading_turn_settle_rate_dps = HEADING_TURN_SETTLE_RATE_DPS;
    config.heading_turn_settle_rpm = HEADING_TURN_SETTLE_RPM;
    config.heading_turn_left_friction_pwm_percent = HEADING_TURN_LEFT_FRICTION_PWM_PERCENT;
    config.heading_turn_right_friction_pwm_percent = HEADING_TURN_RIGHT_FRICTION_PWM_PERCENT;
    config.odometry_track_width_mm = ODOMETRY_TRACK_WIDTH_MM;
    config.navigation_use_ekf = NAVIGATION_USE_EKF;
    config.distance_drive_max_speed_percent = DISTANCE_DRIVE_MAX_SPEED_PERCENT;
    config.distance_drive_acceleration_mps2 = DISTANCE_DRIVE_ACCELERATION_MPS2;
    config.distance_drive_deceleration_mps2 = DISTANCE_DRIVE_DECELERATION_MPS2;
    config.distance_drive_min_speed_percent = DISTANCE_DRIVE_MIN_SPEED_PERCENT;
    config.distance_drive_left_friction_pwm_percent = DISTANCE_DRIVE_LEFT_FRICTION_PWM_PERCENT;
    config.distance_drive_right_friction_pwm_percent = DISTANCE_DRIVE_RIGHT_FRICTION_PWM_PERCENT;
    config.distance_drive_tolerance_mm = DISTANCE_DRIVE_TOLERANCE_MM;
    config.distance_drive_settle_rpm = DISTANCE_DRIVE_SETTLE_RPM;
    config.distance_drive_settle_ms = DISTANCE_DRIVE_SETTLE_MS;
    config.motion_allowed = []() {
        const auto state = Application::GetInstance().GetDeviceState();
        return state != kDeviceStateUpgrading && state != kDeviceStateFatalError &&
               state != kDeviceStateWifiConfiguring;
    };
    config.schedule = [](std::function<void()> callback) {
        Application::GetInstance().Schedule(std::move(callback));
    };
    config.play_proximity_warning = []() {
        auto& app = Application::GetInstance();
        if (app.CanEnterSleepMode())
            app.PlaySound(Lang::Sounds::OGG_POPUP);
    };
    return config;
}

#if HEADING_PD_DEBUG || ODOMETRY_DEBUG
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

#endif

#if FC03_DEBUG
constexpr char kTag[] = "FC03";

void StartFc03Monitor() {
    static TaskHandle_t task = nullptr;
    if (task != nullptr)
        return;
    const auto result = xTaskCreate(
        [](void*) {
            uint64_t previous_left = 0, previous_right = 0;
            int64_t previous_at = esp_timer_get_time();
            while (true) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                const auto status = GetL298nMotorStatus().encoder;
                if (!status.ready) {
                    ESP_LOGE(kTag, "Encoder unavailable: %s", esp_err_to_name(status.error));
                    vTaskDelete(nullptr);
                    return;
                }
                const int64_t now = esp_timer_get_time();
                ESP_LOGI(kTag,
                         "dt=%.2fs D0 L/R=%d/%d cmd=%d/%d total=%" PRIu64 "/%" PRIu64
                         " delta=%" PRIu64 "/%" PRIu64 " Hz=%.1f/%.1f",
                         (now - previous_at) / 1000000.0, status.left.level, status.right.level,
                         status.left.command, status.right.command, status.left.pulses,
                         status.right.pulses, status.left.pulses - previous_left,
                         status.right.pulses - previous_right, status.left.hz, status.right.hz);
                if (status.calibrated) {
                    ESP_LOGI(kTag,
                             "RPM L/R=%.1f/%.1f wheel_m=%.3f/%.3f travel_est_m=%.3f complete=%d",
                             status.left.rpm, status.right.rpm, status.left.distance_m,
                             status.right.distance_m, status.travel_m, status.distance_complete);
                } else {
                    ESP_LOGI(kTag,
                             "UNCALIBRATED: metres/RPM unavailable; set the 4 FC03 calibration "
                             "values in config/robot_hardware_config.h. "
                             "Use total differences to count wheel turns.");
                }
                if (status.left.no_pulses || status.right.no_pulses) {
                    ESP_LOGW(kTag,
                             "No pulses for >=2s while commanded: left=%d right=%d. Check D0, "
                             "disc alignment, sensitivity (if adjustable), or stalled motor.",
                             status.left.no_pulses, status.right.no_pulses);
                }
                if (!status.distance_complete) {
                    ESP_LOGW(kTag,
                             "Unknown-direction pulses L/R=%" PRIu64 "/%" PRIu64
                             "; wheel totals include them, robot travel excludes them. "
                             "Reboot after manual calibration before a driving test.",
                             status.left.unknown_direction_pulses,
                             status.right.unknown_direction_pulses);
                }
                previous_left = status.left.pulses;
                previous_right = status.right.pulses;
                previous_at = now;
            }
        },
        "fc03_monitor", 3072, nullptr, 1, &task);
    if (result != pdPASS) {
        task = nullptr;
        ESP_LOGE(kTag, "Could not start monitor logging task; telemetry still available over HTTP");
    }
}
#endif

}  // namespace

void InitializeRobotBoard() {
    InitializeL298nController(MakeMotorControllerConfig());
    ruby_robot::WebServerConfig web_config;
    web_config.read_network = []() {
        auto& wifi = WifiManager::GetInstance();
        return ruby_robot::WebNetworkStatus{wifi.IsConnected(), wifi.GetRssi(), wifi.GetIpAddress()};
    };
    web_config.request_wake = []() {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateListening)
            return ruby_robot::WebWakeResult::AlreadyListening;
        if (app.GetDeviceState() != kDeviceStateIdle)
            return ruby_robot::WebWakeResult::Unavailable;
        app.Schedule([]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle)
                app.WakeWordInvoke("Hi ESP");
        });
        return ruby_robot::WebWakeResult::Requested;
    };
    web_config.append_telemetry = [](cJSON* parent, const L298nMotorStatus& motor) {
        return AddFc03EncoderJson(parent, motor.encoder) &&
               AddOdometryJson(parent, motor.odometry, motor.ekf,
                               motor.command.distance_drive, motor.command.navigate);
    };
    InitializeWebServer(web_config);
#if HEADING_PD_DEBUG || ODOMETRY_DEBUG
    if (xTaskCreate(ControlMonitorTask, "control_monitor", 4096, nullptr, 2, nullptr) != pdPASS)
        ESP_LOGW("MotorControl", "Could not start control monitor");
#endif
#if FC03_DEBUG
    StartFc03Monitor();
#endif
    RegisterL298nMcpTools();
    ruby_robot::TelegramConfig telegram_config;
    telegram_config.bot_token = L298N_TELEGRAM_BOT_TOKEN;
    telegram_config.chat_id = L298N_TELEGRAM_CHAT_ID;
    telegram_config.can_send = []() {
        const auto state = Application::GetInstance().GetDeviceState();
        return state != kDeviceStateUpgrading && state != kDeviceStateFatalError;
    };
    if (InitializeTelegram(telegram_config))
        RegisterTelegramMcpTools();
}

void UpdateRobotNetwork(bool connected) {
    UpdateWebServerNetwork(connected);
    UpdateTelegramNetwork(connected);
}
