#include "odometry_json.h"

#include <cmath>

namespace {
bool AddMetric(cJSON* json, const char* name, double value, bool valid) {
    return valid ? cJSON_AddNumberToObject(json, name, value) != nullptr
                 : cJSON_AddNullToObject(json, name) != nullptr;
}
}  // namespace

bool AddOdometryJson(cJSON* parent, const ruby_robot::OdometrySnapshot& pose,
                     const ruby_robot::EkfSnapshot& ekf,
                     const ruby_robot::DistanceDriveSnapshot& command,
                     const ruby_robot::NavigateSnapshot& navigation) {
    auto* odometry = cJSON_AddObjectToObject(parent, "odometry");
    if (!odometry || !cJSON_AddBoolToObject(odometry, "valid", pose.valid) ||
        !cJSON_AddBoolToObject(odometry, "initialized", pose.initialized) ||
        !cJSON_AddStringToObject(odometry, "fault", pose.fault) ||
        !cJSON_AddStringToObject(odometry, "direction_source", "motor_command") ||
        !cJSON_AddNumberToObject(odometry, "sampled_at_ms", pose.sampled_at_us / 1000) ||
        !cJSON_AddBoolToObject(odometry, "wheel_heading_valid", pose.wheel_heading_valid) ||
        !AddMetric(odometry, "x_m", pose.x_m, pose.valid) ||
        !AddMetric(odometry, "y_m", pose.y_m, pose.valid) ||
        !AddMetric(odometry, "theta_rad", pose.theta_rad, pose.valid) ||
        !AddMetric(odometry, "left_m", pose.left_m, pose.valid) ||
        !AddMetric(odometry, "right_m", pose.right_m, pose.valid) ||
        !AddMetric(odometry, "centre_m", pose.centre_m, pose.valid) ||
        !AddMetric(odometry, "velocity_mps", pose.velocity_mps, pose.valid) ||
        !AddMetric(odometry, "dt_s", pose.dt_s, pose.valid) ||
        !AddMetric(odometry, "wheel_theta_rad", pose.wheel_theta_rad, pose.wheel_heading_valid))
        return false;
    auto* filter = cJSON_AddObjectToObject(parent, "ekf");
    if (!filter || !cJSON_AddBoolToObject(filter, "valid", ekf.valid) ||
        !cJSON_AddBoolToObject(filter, "initialized", ekf.initialized) ||
        !cJSON_AddStringToObject(filter, "fault", ekf.fault) ||
        !cJSON_AddNumberToObject(filter, "sampled_at_ms", ekf.sampled_at_us / 1000) ||
        !AddMetric(filter, "x_m", ekf.x_m, ekf.valid) ||
        !AddMetric(filter, "y_m", ekf.y_m, ekf.valid) ||
        !AddMetric(filter, "theta_rad", ekf.theta_rad, ekf.valid) ||
        !AddMetric(filter, "sigma_x_m", std::sqrt(ekf.variance_x_m2), ekf.valid) ||
        !AddMetric(filter, "sigma_y_m", std::sqrt(ekf.variance_y_m2), ekf.valid) ||
        !AddMetric(filter, "sigma_theta_rad", std::sqrt(ekf.variance_theta_rad2), ekf.valid))
        return false;
    auto* nav = cJSON_AddObjectToObject(parent, "navigation");
    if (!nav || !cJSON_AddStringToObject(nav, "state", navigation.state) ||
        !cJSON_AddStringToObject(nav, "owner", navigation.owner) ||
        !cJSON_AddStringToObject(nav, "fault", navigation.fault) ||
        !cJSON_AddNumberToObject(nav, "goal_x_m", navigation.goal_x_m) ||
        !cJSON_AddNumberToObject(nav, "goal_y_m", navigation.goal_y_m) ||
        !cJSON_AddNumberToObject(nav, "remaining_m", navigation.remaining_m) ||
        !cJSON_AddNumberToObject(nav, "heading_error_rad", navigation.heading_error_rad) ||
        !cJSON_AddNumberToObject(nav, "speed_percent", navigation.speed_percent) ||
        !cJSON_AddNumberToObject(nav, "remaining_ms", navigation.remaining_ms))
        return false;
    auto* drive = cJSON_AddObjectToObject(parent, "distance_drive");
    return drive && cJSON_AddStringToObject(drive, "state", command.state) &&
           cJSON_AddStringToObject(drive, "owner", command.owner) &&
           cJSON_AddStringToObject(drive, "fault", command.fault) &&
           cJSON_AddNumberToObject(drive, "speed_percent", command.speed_percent) &&
           cJSON_AddNumberToObject(drive, "timeout_seconds", command.timeout_seconds) &&
           cJSON_AddNumberToObject(drive, "elapsed_ms", command.elapsed_ms) &&
           cJSON_AddNumberToObject(drive, "remaining_ms", command.remaining_ms) &&
           cJSON_AddNumberToObject(drive, "target_m", command.target_m) &&
           cJSON_AddNumberToObject(drive, "progress_m", command.progress_m) &&
           cJSON_AddNumberToObject(drive, "error_m", command.error_m) &&
           cJSON_AddNumberToObject(drive, "position_error_m", command.position_error_m);
}
