#include "l298n_internal.h"

namespace ruby_robot {

void L298nController::StopLocked() {
    if (distance_move_state_ == DistanceMoveState::kPending ||
        distance_move_state_ == DistanceMoveState::kMoving)
        distance_move_state_ = DistanceMoveState::kInterrupted;
    web_joystick_active_ = false;
    StopOutputsLocked();
}

void L298nController::UpdateYawEstimateLocked(int64_t now) {
    const auto encoder = encoder_.Snapshot();
    // Unknown-direction pulses can permanently clear distance_complete. For yaw,
    // only use signed wheel travel while both wheel directions are commanded.
    const bool encoder_valid = encoder.ready && encoder.calibrated && encoder.left.command != 0 &&
                               encoder.right.command != 0 && !encoder.left.no_pulses &&
                               !encoder.right.no_pulses;
    const double left_delta = encoder.left.signed_distance_m - last_left_signed_m_;
    const double right_delta = encoder.right.signed_distance_m - last_right_signed_m_;
    last_left_signed_m_ = encoder.left.signed_distance_m;
    last_right_signed_m_ = encoder.right.signed_distance_m;
    const double encoder_delta_degrees =
        (right_delta - left_delta) * 1000.0 / ROBOT_TRACK_WIDTH_MM * 180.0 / kPi;
    if (encoder_valid)
        encoder_yaw_degrees_ += encoder_delta_degrees;
    else
        encoder_yaw_degrees_ = yaw_degrees_;
    const auto imu = GetMpu6050Status();
    const bool fresh = imu.ready && imu.sampled_at_us > 0 &&
                       now - imu.sampled_at_us <= kImuStaleUs &&
                       imu.sampled_at_us - now <= kImuStaleUs;
    const double dt = last_heading_update_us_ > 0
                          ? std::clamp((now - last_heading_update_us_) / 1000000.0, 0.0, 0.05)
                          : kTickUs / 1000000.0;
    last_heading_update_us_ = now;
    if (!fresh) {
        imu_ready_ = false;
        gyro_z_dps_ = 0;
        last_imu_sample_us_ = 0;
        if (!web_manual_ && motion_ != Motion::kStop &&
            (distance_move_state_ == DistanceMoveState::kPending ||
             distance_move_state_ == DistanceMoveState::kMoving ||
             autonomy_mode_ != AutonomyMode::kManual)) {
            distance_move_state_ = DistanceMoveState::kInterrupted;
            StopLocked();
            DisableAutonomyLocked();
        }
        return;
    }
    gyro_z_dps_ = imu.gyro_z_dps;
    if (!imu_ready_) {
        encoder_yaw_degrees_ = yaw_degrees_;
        imu_ready_ = true;
        last_imu_sample_us_ = imu.sampled_at_us;
        if (heading_hold_active_)
            heading_target_degrees_ = yaw_degrees_;
        return;
    }
    double gyro_delta_degrees = 0;
    if (imu.sampled_at_us > last_imu_sample_us_) {
        const double sample_dt =
            std::min(0.05, (imu.sampled_at_us - last_imu_sample_us_) / 1000000.0);
        gyro_delta_degrees = gyro_z_dps_ * sample_dt;
        last_imu_sample_us_ = imu.sampled_at_us;
    }
    const double predicted = yaw_degrees_ + gyro_delta_degrees;
    const double correction_time =
        angle_turn_ && (distance_move_state_ == DistanceMoveState::kMoving || brake_active_)
            ? kTurningCorrectionTimeSeconds
            : kHeadingCorrectionTimeSeconds;
    yaw_degrees_ =
        predicted +
        (encoder_valid ? dt / (correction_time + dt) * (encoder_yaw_degrees_ - predicted) : 0.0);
    if (!encoder_valid)
        encoder_yaw_degrees_ = yaw_degrees_;
    if (angle_turn_ && turn_uses_imu_) {
        turn_progress_degrees_ =
            std::max(0.0, turn_direction_ * (yaw_degrees_ - turn_start_yaw_degrees_));
    }
}

void L298nController::UpdateYawControlLocked(int64_t now) {
    if (last_heading_control_us_ != 0 && now - last_heading_control_us_ < kHeadingControlUpdateUs)
        return;
    last_heading_control_us_ = now;
    if (!moving_ || !imu_ready_) {
        if (heading_correction_percent_ != 0 && moving_) {
            heading_correction_percent_ = 0;
            if (!ApplySpeedLocked())
                StopLocked();
        }
        return;
    }
    if (angle_turn_ && turn_uses_imu_) {
        const double remaining = std::max(0.0, turn_target_degrees_ - turn_progress_degrees_);
        const double rate = std::max(0.0, turn_direction_ * gyro_z_dps_);
        // Keep full turn power until the final angle window, then approach 55%.
        const int cruising_pwm = ActiveSpeedLocked();
        const double speed_fraction =
            std::clamp(kTurnKp * remaining / kTurnSlowdownWindowDegrees, 0.0, 1.0);
        const double requested = kTurnMinimumPwmPercent +
                                 (cruising_pwm - kTurnMinimumPwmPercent) * speed_fraction -
                                 kTurnKd * rate * (1.0 - speed_fraction);
        turn_pwm_percent_ = std::clamp(static_cast<int>(std::lround(requested)),
                                       kTurnMinimumPwmPercent, cruising_pwm);
        if (!ApplySpeedLocked())
            StopLocked();
        return;
    }
    if (!heading_hold_active_ || web_joystick_active_ ||
        (motion_ != Motion::kForward && motion_ != Motion::kBackward))
        return;
    const double error = heading_target_degrees_ - yaw_degrees_;
    const double requested = kHeadingKp * error - kHeadingKd * gyro_z_dps_;
    heading_correction_percent_ =
        std::clamp(static_cast<int>(std::lround(requested)), -kHeadingMaxCorrectionPercent,
                   kHeadingMaxCorrectionPercent);
    if (std::abs(error) < 0.5 && std::abs(gyro_z_dps_) < 2.0)
        heading_correction_percent_ = 0;
    if (!ApplySpeedLocked())
        StopLocked();
}

void L298nController::UpdateDistanceMoveLocked() {
    if (distance_move_state_ != DistanceMoveState::kPending &&
        distance_move_state_ != DistanceMoveState::kMoving)
        return;
    const auto encoder = encoder_.Snapshot();
    if (!encoder.ready || !encoder.calibrated) {
        distance_move_state_ = DistanceMoveState::kEncoderError;
        StopLocked();
        return;
    }
    if (distance_move_state_ != DistanceMoveState::kMoving)
        return;
    if (route_active_ && motion_ == Motion::kForward &&
        (!sensor_valid_ ||
         esp_timer_get_time() - last_sensor_update_us_ > kSensorStaleMs * 1000LL)) {
        distance_move_state_ = DistanceMoveState::kInterrupted;
        StopLocked();
        return;
    }
    if (motion_ == Motion::kForward && sensor_valid_ &&
        esp_timer_get_time() - last_sensor_update_us_ <= kSensorStaleMs * 1000LL &&
        measured_distance_cm_ <= ScaleDistanceForSpeed(obstacle_distance_cm_, speed_percent_)) {
        distance_move_state_ = DistanceMoveState::kObstacle;
        StopLocked();
        return;
    }
    const double left_cm =
        std::max(0.0, (encoder.left.distance_m - distance_move_start_left_m_) * 100.0);
    const double right_cm =
        std::max(0.0, (encoder.right.distance_m - distance_move_start_right_m_) * 100.0);
    if (!angle_turn_) {
        distance_move_progress_cm_ = std::min(left_cm, right_cm);
    }
    const double per_wheel_target_cm =
        ROBOT_TRACK_WIDTH_MM / 20.0 * turn_target_degrees_ * kPi / 180.0;
    const double signed_imu_progress = turn_direction_ * (yaw_degrees_ - turn_start_yaw_degrees_);
    const double encoder_turn_progress =
        (left_cm + right_cm) * 10.0 / ROBOT_TRACK_WIDTH_MM * 180.0 / kPi;
    if (angle_turn_ && signed_imu_progress < -5.0 && encoder_turn_progress > 10.0) {
        // A reversed sensor Z axis must not command a full timeout-length spin.
        imu_axis_mismatch_ = true;
        distance_move_state_ = DistanceMoveState::kInterrupted;
        StopLocked();
        return;
    }
    const double rate = std::max(0.0, turn_direction_ * gyro_z_dps_);
    const double lead = std::min(kTurnStopLeadMaxDegrees, rate * kTurnStopLeadSeconds);
    if (encoder.left.no_pulses || encoder.right.no_pulses) {
        distance_move_state_ = DistanceMoveState::kNoPulses;
        StopLocked();
        return;
    }
    const bool turn_complete = turn_progress_degrees_ >= turn_target_degrees_ ||
                               (turn_progress_degrees_ > 0 && rate > 5.0 &&
                                turn_target_degrees_ - turn_progress_degrees_ <= lead);
    if (angle_turn_ && turn_complete &&
        (left_cm < per_wheel_target_cm * 0.25 || right_cm < per_wheel_target_cm * 0.25)) {
        distance_move_state_ = DistanceMoveState::kEncoderError;
        StopLocked();
        return;
    }
    if (angle_turn_ ? turn_complete : distance_move_progress_cm_ >= distance_move_target_cm_) {
        distance_move_state_ = DistanceMoveState::kCompleted;
        web_joystick_active_ = false;
        BeginDistanceBrakeLocked(esp_timer_get_time());
    }
}

void L298nController::UpdateDistanceSpeedProfileLocked(int64_t now) {
    if (angle_turn_ || distance_move_state_ != DistanceMoveState::kMoving || !moving_ ||
        now - distance_profile_update_us_ < kDistanceRampUpdateUs)
        return;

    const int target_speed = route_active_
                                 ? std::min(std::max(speed_percent_, kDistanceMinimumDrivePercent),
                                            kRouteDriveSpeedPercent)
                                 : std::max(speed_percent_, kDistanceMinimumDrivePercent);
    const int start_speed = std::min(target_speed, kDistanceRampStartSpeedPercent);
    const int crawl_speed = std::min(target_speed, kDistanceCrawlSpeedPercent);
    const double total_distance = distance_move_target_cm_;
    const double progress = std::clamp(distance_move_progress_cm_, 0.0, total_distance);
    const double remaining = total_distance - progress;
    // Short moves get a triangular ramp; longer moves reach the requested speed
    // and begin slowing down over the final 8 cm.
    const double peak_fraction = std::min(1.0, total_distance / (2.0 * kDistanceRampLengthCm));
    const double acceleration_fraction = std::min(1.0, progress / kDistanceRampLengthCm);
    const double deceleration_fraction = std::min(1.0, remaining / kDistanceRampLengthCm);
    const double acceleration_limit = std::min(peak_fraction, acceleration_fraction);
    const double deceleration_limit = std::min(peak_fraction, deceleration_fraction);
    const int accelerating_speed =
        start_speed +
        static_cast<int>(std::lround((target_speed - start_speed) * acceleration_limit));
    const int decelerating_speed =
        crawl_speed +
        static_cast<int>(std::lround((target_speed - crawl_speed) * deceleration_limit));
    const int next_speed = std::min(accelerating_speed, decelerating_speed);
    distance_profile_update_us_ = now;
    if (next_speed != distance_profile_speed_percent_) {
        distance_profile_speed_percent_ = next_speed;
        if (!ApplySpeedLocked())
            StopLocked();
    }
}

int L298nController::ActiveSpeedLocked() const {
    if ((motion_ == Motion::kLeft || motion_ == Motion::kRight) && !web_joystick_active_)
        return std::max(web_manual_ ? web_speed_percent_ : speed_percent_, kTurnSpeedPercent);
    if (distance_move_state_ == DistanceMoveState::kMoving && distance_profile_speed_percent_ > 0)
        return distance_profile_speed_percent_;
    if (web_manual_)
        return web_speed_percent_;
    if (autonomy_mode_ != AutonomyMode::kManual)
        return autonomy_speed_percent_;
    return speed_percent_;
}

bool L298nController::PrepareMotionLocked(Motion motion) {
#ifdef CONFIG_PM_ENABLE
    if (!Check(esp_pm_lock_acquire(sleep_lock_)))
        return false;
    sleep_lock_held_ = true;
#endif
    motion_ = motion;
    angle_turn_ = false;
    turn_uses_imu_ = false;
    turn_direction_ = 0;
    turn_target_degrees_ = 0;
    turn_progress_degrees_ = 0;
    turn_pwm_percent_ = kTurnSpeedPercent;
    heading_hold_active_ = motion == Motion::kForward || motion == Motion::kBackward;
    heading_target_degrees_ = yaw_degrees_;
    heading_correction_percent_ = 0;
    start_at_us_ = esp_timer_get_time() + kDirectionDelayUs;
    deadline_us_ = 0;
    return true;
}

bool L298nController::PrepareDistanceMoveLocked(Motion motion, int distance_cm) {
    const auto encoder = encoder_.Snapshot();
    if ((motion != Motion::kForward && motion != Motion::kBackward) || distance_cm < 1 ||
        distance_cm > kMaxDistanceMoveCm || !encoder.ready || !encoder.calibrated || !imu_ready_ ||
        !PrepareMotionLocked(motion))
        return false;
    deadline_us_ = start_at_us_ + DistanceMoveTimeoutMs(distance_cm) * 1000LL;
    distance_move_target_cm_ = distance_cm;
    distance_move_progress_cm_ = 0;
    distance_profile_speed_percent_ = 0;
    distance_profile_update_us_ = 0;
    distance_move_state_ = DistanceMoveState::kPending;
    return true;
}

bool L298nController::PrepareAngleTurnLocked(Motion motion, int degrees) {
    const auto encoder = encoder_.Snapshot();
    if ((motion != Motion::kLeft && motion != Motion::kRight) || degrees < 1 ||
        degrees > kMaxTurnAngleDegrees || !encoder.ready || !encoder.calibrated || !imu_ready_ ||
        !PrepareMotionLocked(motion))
        return false;
    angle_turn_ = true;
    turn_uses_imu_ = true;
    deadline_us_ = start_at_us_ + kTurnSafetyTimeoutMs * 1000LL;
    imu_axis_mismatch_ = false;
    turn_direction_ = motion == Motion::kLeft ? 1 : -1;
    turn_start_yaw_degrees_ = yaw_degrees_;
    turn_target_degrees_ = degrees;
    turn_progress_degrees_ = 0;
    distance_move_target_cm_ = 0;
    distance_move_progress_cm_ = 0;
    distance_move_state_ = DistanceMoveState::kPending;
    return true;
}

bool L298nController::MotionAllowed() {
    const auto state = Application::GetInstance().GetDeviceState();
    return state != kDeviceStateUpgrading && state != kDeviceStateFatalError &&
           state != kDeviceStateWifiConfiguring;
}

void L298nController::Tick() {
    std::lock_guard lock(mutex_);
    const int64_t now = esp_timer_get_time();
    // Also sample at rest/while joystick owns the outputs, before any early return.
    encoder_.Update(now);
    UpdateYawEstimateLocked(now);
    if (brake_active_) {
        if (now >= brake_until_us_)
            StopOutputsLocked();
        return;
    }
    UpdateDistanceMoveLocked();
    if (brake_active_)
        return;
    UpdateDistanceSpeedProfileLocked(now);
    UpdateSpeedPiLocked(now);
    UpdateYawControlLocked(now);
    if (web_session_ != 0 && (now >= web_deadline_us_ || !MotionAllowed())) {
        EndWebControlLocked();
    }
    const auto state = Application::GetInstance().GetDeviceState();
    if (state == kDeviceStateUpgrading || state == kDeviceStateFatalError ||
        state == kDeviceStateWifiConfiguring) {
        CancelRouteLocked();
        DisableAutonomyLocked();
        if (motion_ != Motion::kStop)
            StopLocked();
        return;
    }
    if (route_active_) {
        if (fault_ != ESP_OK || now >= route_deadline_us_) {
            CancelRouteLocked();
            return;
        }
        if (distance_move_state_ == DistanceMoveState::kCompleted) {
            if (route_index_ + 1 == route_count_) {
                route_active_ = false;
                route_state_ = RouteState::kComplete;
                manual_override_ = response_active_;
            } else {
                ++route_index_;
                if (!StartRouteStepLocked())
                    CancelRouteLocked();
            }
            return;
        }
        if (distance_move_state_ != DistanceMoveState::kPending &&
            distance_move_state_ != DistanceMoveState::kMoving) {
            CancelRouteLocked();
            return;
        }
    }
    // The web session owns direct wheel commands until release or expiry.
    if (web_manual_ && web_joystick_active_)
        return;
    if (autonomy_mode_ != AutonomyMode::kManual) {
        const auto encoder = encoder_.Snapshot();
        if (!imu_ready_ || !encoder.ready || !encoder.calibrated ||
            (moving_ && (encoder.left.no_pulses || encoder.right.no_pulses))) {
            DisableAutonomyLocked();
            StopLocked();
            return;
        }
    }
    if (autonomy_mode_ != AutonomyMode::kManual &&
        (invalid_sensor_samples_ >= 2 || now - last_sensor_update_us_ > kSensorStaleMs * 1000LL)) {
        avoid_clear_samples_ = 0;
        if (autonomy_mode_ == AutonomyMode::kObstacleAvoidance &&
            avoid_state_ != AvoidState::kForward && avoid_state_ != AvoidState::kBlocked)
            BlockAvoidLocked();
        if (motion_ != Motion::kStop)
            StopLocked();
        return;
    }
    if (autonomy_mode_ == AutonomyMode::kObstacleAvoidance && AvoidMotionPhaseLocked()) {
        if (distance_move_state_ == DistanceMoveState::kCompleted && !brake_active_)
            FinishAvoidMotionLocked(now);
        else if (distance_move_state_ != DistanceMoveState::kPending &&
                 distance_move_state_ != DistanceMoveState::kMoving &&
                 distance_move_state_ != DistanceMoveState::kCompleted)
            BlockAvoidLocked();
        return;
    }
    if (motion_ == Motion::kStop)
        return;
    if (deadline_us_ > 0 && now >= deadline_us_) {
        if (distance_move_state_ == DistanceMoveState::kPending ||
            distance_move_state_ == DistanceMoveState::kMoving)
            distance_move_state_ = DistanceMoveState::kTimeout;
        StopLocked();
    } else if (!moving_ && now >= start_at_us_) {
        if (route_active_ && motion_ == Motion::kForward &&
            (!sensor_valid_ || now - last_sensor_update_us_ > kSensorStaleMs * 1000LL ||
             measured_distance_cm_ <=
                 ScaleDistanceForSpeed(obstacle_distance_cm_, speed_percent_))) {
            distance_move_state_ = DistanceMoveState::kInterrupted;
            CancelRouteLocked();
            return;
        }
        if (!ApplyDirectionLocked()) {
            StopLocked();
            return;
        }
        if (distance_move_state_ == DistanceMoveState::kPending) {
            encoder_.Update(esp_timer_get_time());
            const auto encoder = encoder_.Snapshot();
            if (!encoder.ready) {
                distance_move_state_ = DistanceMoveState::kEncoderError;
                StopLocked();
                return;
            }
            distance_move_start_left_m_ = encoder.left.distance_m;
            distance_move_start_right_m_ = encoder.right.distance_m;
            distance_move_state_ = DistanceMoveState::kMoving;
            distance_profile_speed_percent_ =
                std::max(kDistanceMinimumDrivePercent,
                         std::min(speed_percent_, kDistanceRampStartSpeedPercent));
            distance_profile_update_us_ = esp_timer_get_time();
        }
        moving_ = true;
        SetMpu6050MotorActive(true);
        ResetSpeedPiLocked();
        if (kSpeedPiCalibrationValid)
            speed_pi_startup_until_us_ = esp_timer_get_time() + kSpeedPiStartupUs;
        if (!ApplySpeedLocked()) {
            StopLocked();
            return;
        }
    }
}

void L298nController::BeginDistanceBrakeLocked(int64_t now) {
    ResetSpeedPiLocked();
    heading_hold_active_ = false;
    heading_correction_percent_ = 0;
    encoder_.SetDrive(0, 0, now);
    bool ok = true;
    // L298N brakes dynamically when EN is high and both direction inputs match.
    ok = Check(gpio_set_level(MOTOR_LEFT_IN1_PIN, 0)) && ok;
    ok = Check(gpio_set_level(MOTOR_LEFT_IN2_PIN, 0)) && ok;
    ok = Check(gpio_set_level(MOTOR_RIGHT_IN1_PIN, 0)) && ok;
    ok = Check(gpio_set_level(MOTOR_RIGHT_IN2_PIN, 0)) && ok;
    for (auto channel : {kLeftChannel, kRightChannel}) {
        ok = Check(ledc_set_duty(kMode, channel, 1023)) && ok;
        ok = Check(ledc_update_duty(kMode, channel)) && ok;
    }
    if (!ok) {
        StopOutputsLocked();
        return;
    }
    brake_active_ = true;
    brake_until_us_ = now + kDistanceBrakeUs;
    motion_ = Motion::kStop;
    moving_ = false;
    web_left_output_percent_ = 0;
    web_right_output_percent_ = 0;
    start_at_us_ = 0;
    deadline_us_ = 0;
}

void L298nController::ResetSpeedPiLocked() {
    speed_pi_integral_[0] = 0;
    speed_pi_integral_[1] = 0;
    speed_pi_output_percent_[0] = 0;
    speed_pi_output_percent_[1] = 0;
    speed_pi_target_rpm_ = 0;
    speed_pi_startup_until_us_ = 0;
    speed_pi_last_update_us_ = 0;
    speed_pi_active_ = false;
}

bool L298nController::ApplySpeedLocked() {
    const int base_speed = ActiveSpeedLocked();
    if (motion_ == Motion::kLeft || motion_ == Motion::kRight) {
        const int turn_pwm =
            angle_turn_ && turn_uses_imu_ && imu_ready_ ? turn_pwm_percent_ : base_speed;
        return ApplyMotorPwmLocked(turn_pwm, turn_pwm);
    }
    int left = base_speed;
    int right = base_speed;
    if (speed_pi_active_) {
        left = speed_pi_output_percent_[0];
        right = speed_pi_output_percent_[1];
    } else if (kSpeedPiCalibrationValid && moving_ && speed_pi_startup_until_us_ > 0 &&
               esp_timer_get_time() < speed_pi_startup_until_us_) {
        left = right = std::max(base_speed, kSpeedPiStartupPwmPercent);
    }
    if (heading_hold_active_ && imu_ready_ && !web_joystick_active_ &&
        (motion_ == Motion::kForward || motion_ == Motion::kBackward)) {
        const int correction = motion_ == Motion::kForward ? heading_correction_percent_
                                                           : -heading_correction_percent_;
        const int left_floor = std::min(left, kSpeedPiMinimumPwmPercent);
        const int right_floor = std::min(right, kSpeedPiMinimumPwmPercent);
        left = std::clamp(left - correction, left_floor, 100);
        right = std::clamp(right + correction, right_floor, 100);
    }
    return ApplyMotorPwmLocked(left, right);
}

void L298nController::UpdateSpeedPiLocked(int64_t now) {
    const bool turning = motion_ == Motion::kLeft || motion_ == Motion::kRight;
    if (!kSpeedPiCalibrationValid || !moving_ || web_manual_ || turning ||
        ActiveSpeedLocked() <= 0) {
        if (!moving_ || web_manual_ || turning)
            ResetSpeedPiLocked();
        return;
    }
    if (now < speed_pi_startup_until_us_)
        return;
    if (speed_pi_last_update_us_ != 0 && now - speed_pi_last_update_us_ < kSpeedPiUpdateUs)
        return;

    const auto encoder = encoder_.Snapshot();
    if (!encoder.ready || !encoder.calibrated) {
        ResetSpeedPiLocked();
        if (!ApplySpeedLocked())
            StopLocked();
        return;
    }

    const double requested_pwm = std::max(ActiveSpeedLocked(), kSpeedPiMinimumPwmPercent);
    const double left_curve_rpm =
        CalibratedRpmAtPwm(requested_pwm, kLeftRpmAtPwm50, kLeftRpmAtPwm70);
    const double right_curve_rpm =
        CalibratedRpmAtPwm(requested_pwm, kRightRpmAtPwm50, kRightRpmAtPwm70);
    const double target_rpm = (left_curve_rpm + right_curve_rpm) * 0.5;
    const double feed_forward[] = {
        CalibratedPwmForRpm(target_rpm, kLeftRpmAtPwm50, kLeftRpmAtPwm70),
        CalibratedPwmForRpm(target_rpm, kRightRpmAtPwm50, kRightRpmAtPwm70)};
    const double measured_rpm[] = {encoder.left.rpm, encoder.right.rpm};
    int outputs[2] = {};
    const double dt = speed_pi_last_update_us_ == 0
                          ? kSpeedPiUpdateUs / 1000000.0
                          : std::min(0.25, (now - speed_pi_last_update_us_) / 1000000.0);

    for (int i = 0; i < 2; ++i) {
        if ((i == 0 && encoder.left.no_pulses) || (i == 1 && encoder.right.no_pulses)) {
            outputs[i] = std::clamp(static_cast<int>(std::lround(feed_forward[i])),
                                    kSpeedPiMinimumPwmPercent, 100);
            speed_pi_integral_[i] = 0;
            continue;
        }
        const double error = target_rpm - measured_rpm[i];
        const double candidate_integral = std::clamp(speed_pi_integral_[i] + error * dt,
                                                     -kSpeedPiIntegralLimit, kSpeedPiIntegralLimit);
        const double raw_output =
            feed_forward[i] + kSpeedPiKp * error + kSpeedPiKi * candidate_integral;
        const double limited_output =
            std::clamp(raw_output, static_cast<double>(kSpeedPiMinimumPwmPercent), 100.0);
        if (raw_output == limited_output || (raw_output > 100.0 && error < 0) ||
            (raw_output < kSpeedPiMinimumPwmPercent && error > 0)) {
            speed_pi_integral_[i] = candidate_integral;
        }
        outputs[i] = static_cast<int>(std::lround(limited_output));
    }

    speed_pi_target_rpm_ = target_rpm;
    speed_pi_output_percent_[0] = outputs[0];
    speed_pi_output_percent_[1] = outputs[1];
    speed_pi_last_update_us_ = now;
    speed_pi_active_ = true;
    if (!ApplySpeedLocked())
        StopLocked();
}

esp_err_t L298nController::MoveDistanceFromCommand(bool forward, int distance_cm,
                                                   const char* owner) {
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
    return StartDistanceMoveCommand(forward ? Motion::kForward : Motion::kBackward, distance_cm,
                                    owner);
#else
    (void)forward;
    (void)distance_cm;
    (void)owner;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

}  // namespace ruby_robot
