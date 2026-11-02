#include "mcp_tools.h"

#include <expected>
#include <string>

#include "../controller/l298n_controller.h"
#include "../serialization/fc03_json.h"
#include "mcp_server.h"

namespace {

ToolResult GetState() {
    const auto status = GetL298nMotorStatus();
    CJsonUniquePtr json(cJSON_CreateObject());
    if (!json || !cJSON_AddStringToObject(json.get(), "motion", status.motion) ||
        !cJSON_AddStringToObject(json.get(), "command_mode", status.command_mode) ||
        !cJSON_AddStringToObject(json.get(), "time_drive_state", status.time_drive_state) ||
        !cJSON_AddStringToObject(json.get(), "time_drive_direction", status.time_drive_direction) ||
        !cJSON_AddStringToObject(json.get(), "time_drive_owner", status.time_drive_owner) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_speed_percent",
                                 status.time_drive_speed_percent) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_duration_seconds",
                                 status.time_drive_duration_seconds) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_elapsed_ms",
                                 status.time_drive_elapsed_ms) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_remaining_ms",
                                 status.time_drive_remaining_ms) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_samples", status.time_drive_samples) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_average_left_rpm",
                                 status.time_drive_average_left_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_average_right_rpm",
                                 status.time_drive_average_right_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_average_left_error_rpm",
                                 status.time_drive_average_left_error_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_average_right_error_rpm",
                                 status.time_drive_average_right_error_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_average_left_pwm_percent",
                                 status.time_drive_average_left_pwm_percent) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_average_right_pwm_percent",
                                 status.time_drive_average_right_pwm_percent) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_stddev_left_rpm",
                                 status.time_drive_stddev_left_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_stddev_right_rpm",
                                 status.time_drive_stddev_right_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "speed_percent", status.speed_percent) ||
        !cJSON_AddNumberToObject(json.get(), "active_speed_percent", status.active_speed_percent) ||
        !cJSON_AddBoolToObject(json.get(), "moving", status.moving) ||
        !cJSON_AddBoolToObject(json.get(), "pending", status.pending) ||
        !cJSON_AddBoolToObject(json.get(), "web_manual_active", status.web_manual) ||
        !cJSON_AddStringToObject(json.get(), "control_owner", status.control_owner) ||
        !cJSON_AddStringToObject(json.get(), "hardware_error",
                                 esp_err_to_name(status.hardware_error)) ||
        !cJSON_AddNumberToObject(json.get(), "left_pwm_percent", status.left_pwm_percent) ||
        !cJSON_AddNumberToObject(json.get(), "right_pwm_percent", status.right_pwm_percent) ||
        !cJSON_AddBoolToObject(json.get(), "speed_pi_calibrated", status.speed_pi_calibrated) ||
        !cJSON_AddBoolToObject(json.get(), "speed_pi_active", status.speed_pi_active) ||
        !cJSON_AddNumberToObject(json.get(), "speed_pi_left_target_rpm",
                                 status.speed_pi_left_target_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "speed_pi_right_target_rpm",
                                 status.speed_pi_right_target_rpm) ||
        !cJSON_AddStringToObject(json.get(), "speed_pi_fault", status.speed_pi_fault) ||
        !cJSON_AddBoolToObject(json.get(), "distance_valid", status.distance_valid) ||
        !cJSON_AddNumberToObject(json.get(), "measured_distance_cm", status.measured_distance_cm) ||
        !cJSON_AddBoolToObject(json.get(), "imu_ready", status.imu_ready) ||
        !cJSON_AddStringToObject(json.get(), "imu_error", esp_err_to_name(status.imu_error)) ||
        !cJSON_AddNumberToObject(json.get(), "gyro_z_dps", status.gyro_z_dps) ||
        !AddFc03EncoderJson(json.get(), status.encoder))
        return std::unexpected("Could not allocate motor status.");
    return json.release();
}

}  // namespace

void RegisterL298nMcpTools() {
    auto& server = McpServer::GetInstance();
    server.AddTool("self.motor.stop", "Stop the motors and release web control.", PropertyList(),
                   [](const PropertyList&) -> ToolResult {
                       StopL298nMotor();
                       return GetState();
                   });
    server.AddTool(
        "self.motor.get_state",
        "Read motor PWM, speed PI targets, FC-03 encoder and MPU6050 gyro telemetry. "
        "Distance moves, measured turns and heading hold are disabled.",
        PropertyList(), [](const PropertyList&) -> ToolResult { return GetState(); });
    server.AddTool(
        "self.motor.run_time_drive",
        "Drive forward, backward, turn left or turn right for a set number of seconds, then stop. "
        "direction: forward, backward, turn_left or turn_right. "
        "Requires calibrated FC-03 encoders; speed_percent is a target scale.",
        PropertyList({Property("direction", kPropertyTypeString).SetMaxLength(10),
                      Property("speed_percent", kPropertyTypeInteger, 10, 100),
                      Property("duration_seconds", kPropertyTypeInteger, 1, 30)}),
        [](const PropertyList& properties) -> ToolResult {
            const auto direction = ruby_robot::ParseTimeDriveDirection(
                properties["direction"].value<std::string>().c_str());
            if (direction == ruby_robot::Motion::kStop)
                return std::unexpected(std::string("Invalid TimeDrive direction"));
            const esp_err_t error = StartL298nTimeDrive(
                direction,
                properties["speed_percent"].value<int>(),
                properties["duration_seconds"].value<int>(), true);
            if (error != ESP_OK)
                return std::unexpected(std::string("TimeDrive unavailable: ") +
                                       esp_err_to_name(error));
            return GetState();
        });
}
