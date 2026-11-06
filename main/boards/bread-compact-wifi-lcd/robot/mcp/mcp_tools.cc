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
        !cJSON_AddStringToObject(json.get(), "time_drive_state", status.command.time_drive.state) ||
        !cJSON_AddStringToObject(json.get(), "time_drive_direction",
                                 status.command.time_drive.direction) ||
        !cJSON_AddStringToObject(json.get(), "time_drive_owner", status.command.time_drive.owner) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_speed_percent",
                                 status.command.time_drive.speed_percent) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_duration_seconds",
                                 status.command.time_drive.duration_seconds) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_angle_degrees",
                                 status.command.time_drive.angle_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_elapsed_ms",
                                 status.command.time_drive.elapsed_ms) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_remaining_ms",
                                 status.command.time_drive.remaining_ms) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_samples",
                                 status.command.time_drive.metrics.samples) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_average_left_rpm",
                                 status.command.time_drive.metrics.average_left_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_average_right_rpm",
                                 status.command.time_drive.metrics.average_right_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_average_left_error_rpm",
                                 status.command.time_drive.metrics.average_left_error_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_average_right_error_rpm",
                                 status.command.time_drive.metrics.average_right_error_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_average_left_pwm_percent",
                                 status.command.time_drive.metrics.average_left_pwm_percent) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_average_right_pwm_percent",
                                 status.command.time_drive.metrics.average_right_pwm_percent) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_stddev_left_rpm",
                                 status.command.time_drive.metrics.stddev_left_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "time_drive_stddev_right_rpm",
                                 status.command.time_drive.metrics.stddev_right_rpm) ||
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
    server.AddTool("self.motor.run_time_drive",
                   "Drive forward or backward for a set number of seconds, or turn left/right by "
                   "angle_degrees. duration_seconds is the maximum turn time. Requires calibrated "
                   "FC-03 encoders and a valid gyro heading.",
                   PropertyList({Property("direction", kPropertyTypeString).SetMaxLength(10),
                                 Property("speed_percent", kPropertyTypeInteger, 10, 100),
                                 Property("duration_seconds", kPropertyTypeInteger, 1, 30),
                                 Property("angle_degrees", kPropertyTypeInteger, 0, 0, 180)}),
                   [](const PropertyList& properties) -> ToolResult {
                       const auto direction = ruby_robot::ParseTimeDriveDirection(
                           properties["direction"].value<std::string>().c_str());
                       if (direction == ruby_robot::Motion::kStop)
                           return std::unexpected(std::string("Invalid TimeDrive direction"));
                       const int angle = properties["angle_degrees"].value<int>();
                       const bool turn = direction == ruby_robot::Motion::kLeft ||
                                         direction == ruby_robot::Motion::kRight;
                       if ((turn && angle < 5) || (!turn && angle != 0))
                           return std::unexpected(
                               std::string("angle_degrees must be 5-180 for turns, "
                                           "or 0 for forward/backward"));
                       const esp_err_t error = StartL298nCommand(ruby_robot::TimeDriveRequest{
                           direction, properties["speed_percent"].value<int>(),
                           properties["duration_seconds"].value<int>(),
                           ruby_robot::TimeDriveOwner::kVoice, angle});
                       if (error != ESP_OK)
                           return std::unexpected(std::string("TimeDrive unavailable: ") +
                                                  esp_err_to_name(error));
                       return GetState();
                   });
}
