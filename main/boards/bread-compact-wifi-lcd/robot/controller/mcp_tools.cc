#include "controller_internal.h"

namespace ruby_robot {

ToolResult L298nController::MoveAngle(Motion motion, const PropertyList& properties) {
    const int degrees = properties["angle_degrees"].value<int>();
    if (degrees < 1 || degrees > kMaxTurnAngleDegrees)
        return std::unexpected("Use angle_degrees 1..180.");
    {
        std::lock_guard lock(mutex_);
        if (web_manual_)
            return std::unexpected("Release web manual control before turning.");
        const auto encoder = encoder_.Snapshot();
        if (!encoder.ready || !encoder.calibrated || !imu_ready_)
            return std::unexpected("Calibrated FC-03 and a ready MPU6050 are required for a turn.");
        if (fault_ != ESP_OK || !MotionAllowed() || speed_percent_ == 0)
            return std::unexpected("Turning is unavailable in the current motor state.");
        DisableAutonomyLocked();
        manual_override_ = true;
        control_owner_ = "voice";
        StopLocked();
        if (!PrepareAngleTurnLocked(motion, degrees))
            return std::unexpected("Could not start the measured turn.");
    }
    return GetState();
}

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
ToolResult L298nController::MoveDistance(Motion motion, const PropertyList& properties) {
    const int distance_cm = properties["distance_cm"].value<int>();
    const int speed = properties["speed_percent"].value<int>();
    if (distance_cm < 1 || distance_cm > kMaxDistanceMoveCm || speed < -1 || speed > 100) {
        return std::unexpected("Use distance_cm 1..1000 and speed_percent -1..100.");
    }
    {
        std::lock_guard lock(mutex_);
        if (web_manual_)
            return std::unexpected(
                "Web manual control is active; release it on the web page first.");
        const auto encoder = encoder_.Snapshot();
        if (!encoder.ready || !encoder.calibrated || !imu_ready_) {
            return std::unexpected("FC-03 and MPU6050 must be ready for a measured move.");
        }
        if (fault_ != ESP_OK || !MotionAllowed())
            return std::unexpected("Movement unavailable or motor hardware error.");
        const int next_speed = speed >= 0 ? speed : speed_percent_;
        if (next_speed == 0)
            return std::unexpected("Set a nonzero motor speed before moving by distance.");
        DisableAutonomyLocked();
        manual_override_ = true;
        control_owner_ = "voice";
        StopLocked();
        speed_percent_ = next_speed;
        if (!PrepareDistanceMoveLocked(motion, distance_cm))
            return std::unexpected("Could not start the measured move.");
    }
    return GetState();
}

esp_err_t L298nController::StartDistanceMoveCommand(Motion motion, int distance_cm,
                                                    const char* owner) {
    if (distance_cm < 1 || distance_cm > kMaxDistanceMoveCm)
        return ESP_ERR_INVALID_ARG;
    std::lock_guard lock(mutex_);
    if (web_manual_)
        return ESP_ERR_INVALID_STATE;
    if (owner != nullptr && strcmp(owner, "web") == 0 && !web_connected_)
        return ESP_ERR_INVALID_STATE;
    const auto encoder = encoder_.Snapshot();
    if (!encoder.ready || !encoder.calibrated || !imu_ready_)
        return ESP_ERR_NOT_SUPPORTED;
    if (fault_ != ESP_OK)
        return fault_;
    if (!MotionAllowed())
        return ESP_ERR_INVALID_STATE;
    if (speed_percent_ == 0)
        return ESP_ERR_INVALID_STATE;
    DisableAutonomyLocked();
    manual_override_ = true;
    control_owner_ = owner != nullptr && strcmp(owner, "web") == 0 ? "web" : "telegram";
    StopLocked();
    if (!PrepareDistanceMoveLocked(motion, distance_cm))
        return ESP_FAIL;
    return ESP_OK;
}
#endif

ToolResult L298nController::SetSpeed(int speed) {
    const auto error = SetSpeedPercent(speed);
    if (error == ESP_ERR_INVALID_ARG) {
        return std::unexpected("Use speed_percent 0..100.");
    }
    if (error == ESP_ERR_INVALID_STATE) {
        return std::unexpected("Web manual control is active; release it on the web page first.");
    }
    if (error != ESP_OK) {
        return std::unexpected("Motor hardware error; inspect wiring and restart.");
    }
    return GetState();
}

ToolResult L298nController::GetState() {
    Motion motion;
    AutonomyMode autonomy_mode;
    int speed;
    int active_speed;
    int follow_distance;
    int obstacle_distance;
    int effective_follow_distance;
    int effective_obstacle_distance;
    int measured_distance;
    bool moving;
    bool web_manual;
    bool distance_valid;
    bool autonomy_blocked;
    int64_t remaining_ms;
    esp_err_t fault;
    bool speed_pi_active;
    bool speed_pi_calibrated;
    double speed_pi_target_rpm;
    int left_pwm_percent;
    int right_pwm_percent;
    Fc03EncoderStatus encoder;
    DistanceMoveState distance_move_state;
    int distance_move_target_cm;
    double distance_move_progress_cm;
    int turn_target_degrees;
    double turn_progress_degrees;
    bool imu_ready;
    esp_err_t imu_error;
    bool imu_axis_mismatch;
    double heading_degrees;
    double gyro_z_dps;
    bool heading_hold_active;
    bool turn_uses_imu;
    const char* control_owner;
    bool warning_sound_enabled;
    {
        std::lock_guard lock(mutex_);
        const int64_t now = esp_timer_get_time();
        motion = motion_;
        autonomy_mode = autonomy_mode_;
        speed = speed_percent_;
        moving = moving_;
        active_speed = moving_ ? ActiveSpeedLocked() : 0;
        follow_distance = follow_distance_cm_;
        obstacle_distance = obstacle_distance_cm_;
        effective_follow_distance = std::min(follow_distance_cm_, kFollowMaxTargetCm);
        effective_obstacle_distance = ScaleDistanceForSpeed(obstacle_distance_cm_, speed_percent_);
        distance_valid = sensor_valid_ && now - last_sensor_update_us_ <= kSensorStaleMs * 1000LL;
        measured_distance = distance_valid ? measured_distance_cm_ : -1;
        autonomy_blocked = avoid_state_ == AvoidState::kBlocked;
        web_manual = web_manual_;
        fault = fault_;
        speed_pi_calibrated = kSpeedPiCalibrationValid;
        speed_pi_active = speed_pi_active_;
        speed_pi_target_rpm = speed_pi_target_rpm_;
        left_pwm_percent = CurrentLeftPwmPercentLocked();
        right_pwm_percent = CurrentRightPwmPercentLocked();
        encoder = encoder_.Snapshot();
        distance_move_state = distance_move_state_;
        distance_move_target_cm = distance_move_target_cm_;
        distance_move_progress_cm = distance_move_progress_cm_;
        turn_target_degrees = turn_target_degrees_;
        turn_progress_degrees = turn_progress_degrees_;
        imu_ready = imu_ready_;
        imu_error = GetMpu6050Status().error;
        imu_axis_mismatch = imu_axis_mismatch_;
        heading_degrees = yaw_degrees_;
        gyro_z_dps = gyro_z_dps_;
        heading_hold_active = heading_hold_active_ && imu_ready_;
        turn_uses_imu = turn_uses_imu_;
        control_owner = ControlOwnerLocked();
        warning_sound_enabled = proximity_warning_enabled_;
        remaining_ms = deadline_us_ > 0 && motion != Motion::kStop
                           ? std::max<int64_t>(0, (deadline_us_ - now + 999) / 1000)
                           : 0;
    }
    CJsonUniquePtr json(cJSON_CreateObject());
    if (!json || !cJSON_AddStringToObject(json.get(), "motion", MotionName(motion)) ||
        !cJSON_AddNumberToObject(json.get(), "speed_percent", speed) ||
        !cJSON_AddNumberToObject(json.get(), "active_speed_percent", active_speed) ||
        !cJSON_AddBoolToObject(json.get(), "speed_pi_calibrated", speed_pi_calibrated) ||
        !cJSON_AddBoolToObject(json.get(), "speed_pi_active", speed_pi_active) ||
        !cJSON_AddNumberToObject(json.get(), "speed_pi_target_rpm", speed_pi_target_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "left_pwm_percent", left_pwm_percent) ||
        !cJSON_AddNumberToObject(json.get(), "right_pwm_percent", right_pwm_percent) ||
        !cJSON_AddBoolToObject(json.get(), "web_manual_active", web_manual) ||
        !cJSON_AddStringToObject(json.get(), "autonomy_mode", AutonomyModeName(autonomy_mode)) ||
        !cJSON_AddStringToObject(json.get(), "control_owner", control_owner) ||
        !cJSON_AddBoolToObject(json.get(), "warning_sound_enabled", warning_sound_enabled) ||
        !cJSON_AddNumberToObject(json.get(), "follow_distance_cm", follow_distance) ||
        !cJSON_AddNumberToObject(json.get(), "obstacle_distance_cm", obstacle_distance) ||
        !cJSON_AddNumberToObject(json.get(), "follow_target_cm", effective_follow_distance) ||
        !cJSON_AddNumberToObject(json.get(), "obstacle_stop_cm", effective_obstacle_distance) ||
        !cJSON_AddNumberToObject(json.get(), "measured_distance_cm", measured_distance) ||
        !cJSON_AddBoolToObject(json.get(), "distance_valid", distance_valid) ||
        !cJSON_AddBoolToObject(json.get(), "autonomy_blocked", autonomy_blocked) ||
        !cJSON_AddBoolToObject(json.get(), "moving", moving) ||
        !cJSON_AddBoolToObject(json.get(), "pending", motion != Motion::kStop && !moving) ||
        !cJSON_AddNumberToObject(json.get(), "remaining_ms", remaining_ms) ||
        !cJSON_AddStringToObject(json.get(), "hardware_error", esp_err_to_name(fault)) ||
        !cJSON_AddStringToObject(json.get(), "distance_move_state",
                                 DistanceMoveStateName(distance_move_state)) ||
        !cJSON_AddNumberToObject(json.get(), "distance_move_target_cm", distance_move_target_cm) ||
        !cJSON_AddNumberToObject(json.get(), "distance_move_progress_cm",
                                 distance_move_progress_cm) ||
        !cJSON_AddNumberToObject(json.get(), "turn_target_degrees", turn_target_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "turn_progress_degrees", turn_progress_degrees) ||
        !cJSON_AddBoolToObject(json.get(), "imu_ready", imu_ready) ||
        !cJSON_AddStringToObject(json.get(), "imu_error", esp_err_to_name(imu_error)) ||
        !cJSON_AddBoolToObject(json.get(), "imu_axis_mismatch", imu_axis_mismatch) ||
        !cJSON_AddNumberToObject(json.get(), "heading_degrees", heading_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "gyro_z_dps", gyro_z_dps) ||
        !cJSON_AddBoolToObject(json.get(), "heading_hold_active", heading_hold_active) ||
        !cJSON_AddBoolToObject(json.get(), "turn_uses_imu", turn_uses_imu) ||
        !AddFc03EncoderJson(json.get(), encoder)) {
        return std::unexpected("Could not allocate motor status.");
    }
    return json.release();  // Ownership passes to MCP.
}

void L298nController::RegisterTools() {
    auto& server = McpServer::GetInstance();
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
    const auto add_distance_move = [this, &server](const char* name, const char* description,
                                                   Motion motion) {
        server.AddTool(
            name,
            std::string(description) +
                " distance_cm is required (1..1000 cm of wheel travel, not HC-SR04 range). "
                "Requires calibrated FC-03 encoders and a ready MPU6050. Stops when "
                "each wheel reaches the target distance. "
                "speed_percent: 1..100 PWM duty, -1 keeps current speed; measured "
                "moves use a 50 percent drive floor. "
                "Forward travel also stops near a detected obstacle. "
                "Stops on missing pulses, sensor failure, or a distance-scaled timeout "
                "(at least 10 seconds, 53 seconds maximum). Reverse travel has no rear sensor; "
                "ensure the path is clear. "
                "Result confirms acceptance, not completion; check distance_move_state and "
                "distance_move_progress_cm via self.motor.get_state. Do not repeat unless "
                "asked.",
            PropertyList({Property("distance_cm", kPropertyTypeInteger, 1, kMaxDistanceMoveCm),
                          Property("speed_percent", kPropertyTypeInteger, -1, -1, 100)}),
            [this, motion](const PropertyList& properties) -> ToolResult {
                return MoveDistance(motion, properties);
            });
    };
    add_distance_move("self.motor.forward_distance", "Drive forward (tiến) by a measured distance.",
                      Motion::kForward);
    add_distance_move("self.motor.backward_distance",
                      "Drive backward (lùi) by a measured distance.", Motion::kBackward);
    add_distance_move("self.motor.forward", "Drive forward (tiến) by a measured distance.",
                      Motion::kForward);
    add_distance_move("self.motor.backward", "Drive backward (lùi) by a measured distance.",
                      Motion::kBackward);
#endif
    const auto add_angle_turn = [this, &server](const char* name, const char* description,
                                                Motion motion) {
        server.AddTool(name,
                       std::string(description) +
                           " angle_degrees: 1..180, default 90. Requires FC-03 and MPU6050, "
                           "braking near the target. Stops on sensor failure or timeout. Result "
                           "confirms acceptance, not completion; inspect distance_move_state and "
                           "turn_progress_degrees.",
                       PropertyList({Property("angle_degrees", kPropertyTypeInteger,
                                              kDefaultTurnAngleDegrees, 1, kMaxTurnAngleDegrees)}),
                       [this, motion](const PropertyList& properties) -> ToolResult {
                           return MoveAngle(motion, properties);
                       });
    };
    add_angle_turn("self.motor.turn_left", "Rotate robot left in place.", Motion::kLeft);
    add_angle_turn("self.motor.turn_right", "Rotate robot right in place.", Motion::kRight);
    add_angle_turn("self.motor.left", "Rotate robot left by a measured angle.", Motion::kLeft);
    add_angle_turn("self.motor.right", "Rotate robot right by a measured angle.", Motion::kRight);
    server.AddTool("self.motor.stop",
                   "Stop robot (dừng lại) now and cancel pending movement. Motors coast; speed "
                   "setting is retained. Also disables follow mode and obstacle avoidance.",
                   PropertyList(), [this](const PropertyList&) -> ToolResult {
                       Stop();
                       return GetState();
                   });
    server.AddTool("self.motor.set_speed",
                   "Set motor PWM speed 0..100 percent (tốc độ). 0 stops. While moving, applies "
                   "immediately to active movement without extending its safety deadline. "
                   "While stopped, only saves speed for the next "
                   "movement; never starts motion. This is not measured wheel speed.",
                   PropertyList({Property("speed_percent", kPropertyTypeInteger, 0, 100)}),
                   [this](const PropertyList& properties) -> ToolResult {
                       return SetSpeed(properties["speed_percent"].value<int>());
                   });
    server.AddTool("self.motor.get_state",
                   "Read commanded motor direction, PWM speed, pending/moving status, "
                   "safety deadline and hardware error. FC-03 encoder reports wheel "
                   "pulses, measured RPM, wheel distance and estimated robot travel since boot "
                   "in metres. Values are null until calibrated/ready. Direction is inferred "
                   "from commands; distance_complete=false means unknown-direction pulses "
                   "were excluded. MPU6050 fields report relative heading, yaw rate, "
                   "readiness, heading hold, and whether the current or last motion "
                   "was an IMU turn. "
                   "Angle turns report "
                   "turn_target_degrees and turn_progress_degrees. Wheel slip and coasting "
                   "still cause error.",
                   PropertyList(),
                   [this](const PropertyList&) -> ToolResult { return GetState(); });
    server.AddTool(
        "self.navigation.set_follow_mode",
        "Enable or disable HC-SR04 follow mode (bật/tắt chế độ đi theo). The robot only "
        "follows an object directly in front and stops when the target is lost. At 10 cm "
        "or closer it reverses until reaching the configured target distance; otherwise "
        "it moves forward when farther than the target plus a 4 cm deadband. The default "
        "target is 30 cm and does not change with speed. "
        "There is no rear obstacle sensor. Enabling disables obstacle avoidance. "
        "distance_cm: 10..200 changes the target before switching mode; -1 keeps the current "
        "target. Defaults off with a 30 cm target after reboot.",
        PropertyList({Property("enabled", kPropertyTypeBoolean),
                      Property("distance_cm", kPropertyTypeInteger, -1, -1, kMaxFollowDistanceCm)}),
        [this](const PropertyList& properties) -> ToolResult {
            const int distance = properties["distance_cm"].value<int>();
            if (distance != -1 && distance < kMinFollowDistanceCm)
                return std::unexpected("Use follow distance 10..200 cm or -1 to keep it.");
            if (distance != -1 && SetFollowDistance(distance) != ESP_OK)
                return std::unexpected("Use follow distance 10..200 cm or -1 to keep it.");
            const auto error = SetFollowMode(properties["enabled"].value<bool>());
            if (error != ESP_OK)
                return std::unexpected(
                    "Follow mode requires FC-03 and MPU6050 and is unavailable during "
                    "web control, a motor fault, or zero speed.");
            return GetState();
        });
    server.AddTool("self.navigation.set_follow_distance",
                   "Set the HC-SR04 follow target distance in centimetres (khoảng cách đi theo), "
                   "10..200. Default is 30 cm and does not change with speed. Changing it "
                   "does not enable follow mode.",
                   PropertyList({Property("distance_cm", kPropertyTypeInteger, kMinFollowDistanceCm,
                                          kMaxFollowDistanceCm)}),
                   [this](const PropertyList& properties) -> ToolResult {
                       const auto error = SetFollowDistance(properties["distance_cm"].value<int>());
                       if (error != ESP_OK)
                           return std::unexpected("Use follow distance 10..200 cm.");
                       return GetState();
                   });
    server.AddTool(
        "self.navigation.set_obstacle_avoidance",
        "Enable or disable autonomous HC-SR04 obstacle avoidance (bật/tắt tự hành né vật "
        "cản). At the effective stop distance the robot reverses 6 cm, probes left, and "
        "drives left immediately if clearance exceeds both 40 cm and the stop distance. "
        "Otherwise it probes right and chooses the farther safe direction, rechecking "
        "left if it returns there. It stops if neither direction is safe or readings fail. "
        "Turns use calibrated FC-03 wheel encoders and MPU6050 yaw; there is no "
        "rear sensor. "
        "Enabling disables follow mode. Defaults "
        "off after reboot. distance_cm: 10..100 changes the stop distance before switching "
        "mode; -1 keeps the current distance (20 cm at 50% PWM after reboot). The effective "
        "stop distance scales proportionally above 50% and never falls below the configured "
        "value at lower speeds.",
        PropertyList(
            {Property("enabled", kPropertyTypeBoolean),
             Property("distance_cm", kPropertyTypeInteger, -1, -1, kMaxObstacleDistanceCm)}),
        [this](const PropertyList& properties) -> ToolResult {
            const int distance = properties["distance_cm"].value<int>();
            if (distance != -1 && distance < kMinObstacleDistanceCm)
                return std::unexpected("Use obstacle distance 10..100 cm or -1 to keep it.");
            if (distance != -1 && SetObstacleDistance(distance) != ESP_OK)
                return std::unexpected("Use obstacle distance 10..100 cm or -1 to keep it.");
            const auto error = SetObstacleAvoidance(properties["enabled"].value<bool>());
            if (error != ESP_OK)
                return std::unexpected(
                    "Obstacle avoidance needs calibrated FC-03 encoders and MPU6050; it is "
                    "unavailable during web control, a motor fault, or zero speed.");
            return GetState();
        });
    server.AddTool(
        "self.navigation.set_obstacle_distance",
        "Set the HC-SR04 obstacle stop distance baseline in centimetres (khoảng cách né vật "
        "cản), 10..100. Default is 20 cm at 50% PWM; the effective stop distance scales "
        "proportionally above 50% and never falls below the configured value at lower speeds. "
        "Changing it does not enable obstacle avoidance.",
        PropertyList({Property("distance_cm", kPropertyTypeInteger, kMinObstacleDistanceCm,
                               kMaxObstacleDistanceCm)}),
        [this](const PropertyList& properties) -> ToolResult {
            const auto error = SetObstacleDistance(properties["distance_cm"].value<int>());
            if (error != ESP_OK)
                return std::unexpected("Use obstacle distance 10..100 cm.");
            return GetState();
        });
    server.AddTool("self.navigation.get_state",
                   "Read HC-SR04 distance, sensor validity, configured and speed-adjusted "
                   "follow/avoid thresholds, current autonomy mode and blocked state.",
                   PropertyList(),
                   [this](const PropertyList&) -> ToolResult { return GetState(); });
}

}  // namespace ruby_robot
