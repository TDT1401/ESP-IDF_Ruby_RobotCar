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
        !AddOdometryJson(json.get(), status.odometry, status.ekf,
                         status.command.distance_drive, status.command.navigate))
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
                   "Includes odometry, EKF pose/uncertainty, navigation goal/state, and "
                   "distance goal/error. "
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
            const int distance_cm = properties["distance_cm"].value<int>();
            const int speed_percent = properties["speed_percent"].value<int>();
            const int timeout_seconds = properties["timeout_seconds"].value<int>();
            esp_err_t error = ESP_ERR_INVALID_ARG;
            if (distance_cm != 0 &&
                distance_cm >= -ruby_robot::MotionGoalCommand::kMaxDistanceCm &&
                distance_cm <= ruby_robot::MotionGoalCommand::kMaxDistanceCm) {
                const L298nLinearRequest request{ruby_robot::DistanceDriveRequest{
                    distance_cm < 0 ? -distance_cm : distance_cm, speed_percent, timeout_seconds,
                    ruby_robot::DistanceDriveOwner::kVoice}};
                error = distance_cm > 0 ? MoveL298nForward(request) : MoveL298nBackward(request);
            }
            if (error != ESP_OK)
                return std::unexpected(std::string("Distance move unavailable: ") +
                                       esp_err_to_name(error));
            return GetState();
        });
    server.AddTool(
        "self.motor.rotate_angle",
        "Turn by a signed relative angle: positive left, negative right, magnitude 5-180 degrees. "
        "Uses gyro feedback and automatic speed, with a 5-second safety timeout.",
        PropertyList({Property("angle_degrees", kPropertyTypeInteger, -180, 180)}),
        [](const PropertyList& properties) -> ToolResult {
            const int angle_degrees = properties["angle_degrees"].value<int>();
            esp_err_t error = ESP_ERR_INVALID_ARG;
            if (angle_degrees != 0 &&
                angle_degrees >= -ruby_robot::MotionGoalCommand::kMaxAngleDegrees &&
                angle_degrees <= ruby_robot::MotionGoalCommand::kMaxAngleDegrees) {
                const L298nTurnRequest request{ruby_robot::AngleTurnRequest{
                    angle_degrees < 0 ? -angle_degrees : angle_degrees, 0,
                    ruby_robot::AngleTurnOwner::kVoice}};
                error = angle_degrees > 0 ? TurnL298nLeft(request) : TurnL298nRight(request);
            }
            if (error != ESP_OK)
                return std::unexpected(std::string("Angle turn unavailable: ") +
                                       esp_err_to_name(error));
            return GetState();
        });
    server.AddTool(
        "self.motor.navigate_to",
        "Drive to a relative x/y goal in centimetres from the last odometry reset. No obstacle "
        "avoidance: stops for a detected front obstacle, invalid pose, or timeout. Uses "
        "the configured pose source for control.",
        PropertyList({Property("x_cm", kPropertyTypeInteger, -1000, 1000),
                      Property("y_cm", kPropertyTypeInteger, -1000, 1000),
                      Property("speed_percent", kPropertyTypeInteger, 40, 18, 70),
                      Property("timeout_seconds", kPropertyTypeInteger, 60, 1, 120)}),
        [](const PropertyList& properties) -> ToolResult {
            const esp_err_t error = StartL298nCommand(ruby_robot::NavigateRequest{
                properties["x_cm"].value<int>() / 100.0,
                properties["y_cm"].value<int>() / 100.0,
                properties["speed_percent"].value<int>(),
                properties["timeout_seconds"].value<int>(), ruby_robot::NavigateOwner::kVoice});
            if (error != ESP_OK)
                return std::unexpected(std::string("Navigation unavailable: ") +
                                       esp_err_to_name(error));
            return GetState();
        });
}
