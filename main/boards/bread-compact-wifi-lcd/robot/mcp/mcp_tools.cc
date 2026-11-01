#include "mcp_tools.h"

#include <expected>

#include "../controller/l298n_controller.h"
#include "../serialization/fc03_json.h"
#include "mcp_server.h"

namespace {

ToolResult GetState() {
    const auto status = GetL298nMotorStatus();
    CJsonUniquePtr json(cJSON_CreateObject());
    if (!json || !cJSON_AddStringToObject(json.get(), "motion", status.motion) ||
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
}
