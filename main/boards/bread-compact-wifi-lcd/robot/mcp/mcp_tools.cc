#include "mcp_tools.h"

#include <expected>
#include <string>

#include "../controller/l298n_controller.h"
#include "../serialization/fc03_json.h"
#include "../serialization/odometry_json.h"
#include "mcp_server.h"

namespace {

ToolResult GetState() {
    const auto status = GetL298nMotorStatus();
    CJsonUniquePtr json(cJSON_CreateObject());
    if (!json || !cJSON_AddStringToObject(json.get(), "motion", status.motion) ||
        !cJSON_AddStringToObject(json.get(), "command_mode", status.command.mode) ||
        !cJSON_AddStringToObject(json.get(), "angle_turn_state", status.command.angle_turn.state) ||
        !cJSON_AddStringToObject(json.get(), "angle_turn_direction",
                                 status.command.angle_turn.direction) ||
        !cJSON_AddStringToObject(json.get(), "angle_turn_owner", status.command.angle_turn.owner) ||
        !cJSON_AddNumberToObject(json.get(), "angle_turn_degrees",
                                 status.command.angle_turn.angle_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "angle_turn_speed_percent",
                                 status.command.angle_turn.speed_percent) ||
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
        !cJSON_AddBoolToObject(json.get(), "heading_valid", status.heading_valid) ||
        !cJSON_AddNumberToObject(json.get(), "heading_degrees", status.heading_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "gyro_z_dps", status.gyro_z_dps) ||
        !cJSON_AddBoolToObject(json.get(), "heading_hold_active", status.heading_hold_active) ||
        !cJSON_AddNumberToObject(json.get(), "heading_target_degrees",
                                 status.heading_target_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "heading_error_degrees",
                                 status.heading_error_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "heading_correction_rpm",
                                 status.heading_correction_rpm) ||
        !cJSON_AddStringToObject(json.get(), "heading_fault", status.heading_fault) ||
        !cJSON_AddNumberToObject(json.get(), "turn_target_degrees", status.turn_target_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "turn_progress_degrees",
                                 status.turn_progress_degrees) ||
        !cJSON_AddBoolToObject(json.get(), "turn_uses_imu", status.turn_uses_imu) ||
        !AddFc03EncoderJson(json.get(), status.encoder) ||
        !AddOdometryJson(json.get(), status.odometry, status.command.distance_drive))
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
    server.AddTool("self.motor.get_state",
                   "Read motor PWM, speed PI targets, FC-03 encoder and relative MPU6050 heading. "
                   "Includes relative odometry x/y/theta and distance goal/error. "
                   "Timed forward/backward commands hold heading; angle turns use gyro feedback.",
                   PropertyList(), [](const PropertyList&) -> ToolResult { return GetState(); });
    server.AddTool("self.motor.reset_odometry",
                   "Set relative pose to (0,0,0). Only while idle, stationary and outside a web "
                   "manual session.",
                   PropertyList(), [](const PropertyList&) -> ToolResult {
                       const esp_err_t error = ResetL298nOdometry();
                       if (error != ESP_OK)
                           return std::unexpected(std::string("Pose reset unavailable: ") +
                                                  esp_err_to_name(error));
                       return GetState();
                   });
    server.AddTool(
        "self.motor.move_distance",
        "Drive a signed distance in centimetres: positive forward, negative backward, magnitude "
        "5-1000 cm. Holds heading and slows before stopping. Requires valid odometry and "
        "stationary "
        "wheels. speed_percent is a ceiling; timeout_seconds is a safety limit.",
        PropertyList({Property("distance_cm", kPropertyTypeInteger, -1000, 1000),
                      Property("speed_percent", kPropertyTypeInteger, 40, 10, 100),
                      Property("timeout_seconds", kPropertyTypeInteger, 60, 1, 120)}),
        [](const PropertyList& properties) -> ToolResult {
            const esp_err_t error = StartL298nCommand(ruby_robot::DistanceDriveRequest{
                properties["distance_cm"].value<int>(), properties["speed_percent"].value<int>(),
                properties["timeout_seconds"].value<int>(),
                ruby_robot::DistanceDriveOwner::kVoice});
            if (error != ESP_OK)
                return std::unexpected(std::string("Distance move unavailable: ") +
                                       esp_err_to_name(error));
            return GetState();
        });
    server.AddTool(
        "self.motor.rotate_angle",
        "Turn by a signed relative angle: positive left, negative right, magnitude 5-180 degrees. "
        "Uses gyro feedback and automatic speed, with a 30-second safety timeout.",
        PropertyList({Property("angle_degrees", kPropertyTypeInteger, -180, 180)}),
        [](const PropertyList& properties) -> ToolResult {
            const esp_err_t error = RotateL298nAngle(properties["angle_degrees"].value<int>());
            if (error != ESP_OK)
                return std::unexpected(std::string("Angle turn unavailable: ") +
                                       esp_err_to_name(error));
            return GetState();
        });
}
