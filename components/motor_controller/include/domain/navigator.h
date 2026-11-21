#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "../commands/motion_goal_command.h"
#include "motion_policy.h"
#include "odometry.h"

namespace ruby_robot {

enum class NavigateOwner { kWeb, kVoice };
enum class NavigateState { kIdle, kTurning, kDriving, kCompleted, kInterrupted, kFault };

struct NavigateRequest {
    double x_m = 0;
    double y_m = 0;
    NavigateOwner owner = NavigateOwner::kVoice;
};

struct NavigateSnapshot {
    const char* state = "idle";
    const char* owner = "none";
    const char* fault = "none";
    double goal_x_m = 0;
    double goal_y_m = 0;
    double remaining_m = 0;
    double heading_error_rad = 0;
    int speed_percent = 0;
    int remaining_ms = 0;
};

// Pure relative-coordinate goal. The controller supplies pose, wheel feedback,
// and the already calibrated distance/speed limits; it owns motor transitions.
class Navigator {
public:
    static constexpr double kHeadingToleranceRad = 3.0 * std::numbers::pi / 180.0;

    static bool Valid(const NavigateRequest& request) {
        return std::isfinite(request.x_m) && std::isfinite(request.y_m) &&
               std::abs(request.x_m) <= 10 && std::abs(request.y_m) <= 10 &&
               (request.owner == NavigateOwner::kWeb || request.owner == NavigateOwner::kVoice);
    }

    bool Start(const NavigateRequest& request, const OdometrySnapshot& pose,
               const DistanceDriveLimits& limits, int64_t now_us) {
        if (!Valid(request) || !ValidPose(pose) ||
            !std::isfinite(limits.metres_per_second_per_percent) ||
            limits.metres_per_second_per_percent <= 0 || !std::isfinite(limits.acceleration_mps2) ||
            limits.acceleration_mps2 <= 0 ||
            !std::isfinite(limits.deceleration_mps2) || limits.deceleration_mps2 <= 0 ||
            limits.min_speed_percent < 1 || limits.max_speed_percent < limits.min_speed_percent ||
            limits.max_speed_percent > 100)
            return false;
        const double distance = std::hypot(request.x_m - pose.x_m, request.y_m - pose.y_m);
        if (!std::isfinite(distance) || distance < kToleranceM)
            return false;
        const double cruise_mps = limits.max_speed_percent * limits.metres_per_second_per_percent;
        if (!std::isfinite(cruise_mps) || cruise_mps <= 0)
            return false;
        request_ = request;
        limits_ = limits;
        started_us_ = last_update_us_ = now_us;
        // Allow time for acceleration, braking and the initial turn, with a bounded margin.
        const double drive_time_s = distance / cruise_mps + cruise_mps / limits.acceleration_mps2 +
                                    cruise_mps / limits.deceleration_mps2;
        const double timeout_s =
            std::clamp(2.0 * drive_time_s + kTurnTimeoutUs / 1000000.0 + 5.0, 15.0, 120.0);
        deadline_us_ = now_us + static_cast<int64_t>(timeout_s * 1000000.0);
        settled_since_us_ = 0;
        braking_ = false;
        remaining_m_ = distance;
        heading_error_rad_ = BearingError(pose);
        state_ = std::abs(heading_error_rad_) > kHeadingToleranceRad ? NavigateState::kTurning
                                                                     : NavigateState::kDriving;
        speed_percent_ = state_ == NavigateState::kTurning ? 0 : limits_.min_speed_percent;
        speed_mps_ = speed_percent_ * limits_.metres_per_second_per_percent;
        fault_ = "none";
        return true;
    }

    bool Update(const OdometrySnapshot& pose, double left_rpm, double right_rpm, int64_t now_us) {
        if (!Active())
            return true;
        if (!ValidPose(pose)) {
            fault_ = pose.valid ? "invalid_pose" : pose.fault;
            return false;
        }
        if (!std::isfinite(left_rpm) || !std::isfinite(right_rpm) || left_rpm < 0 || right_rpm < 0) {
            fault_ = "encoder_error";
            return false;
        }
        remaining_m_ = std::hypot(request_.x_m - pose.x_m, request_.y_m - pose.y_m);
        heading_error_rad_ = BearingError(pose);
        if (state_ == NavigateState::kTurning) {
            if (now_us - started_us_ >= kTurnTimeoutUs) {
                fault_ = "turn_timeout";
                return false;
            }
            return true;
        }
        braking_ = braking_ || remaining_m_ <= kToleranceM;
        if (braking_) {
            speed_percent_ = 0;
            speed_mps_ = 0;
            if (left_rpm <= kSettleRpm && right_rpm <= kSettleRpm) {
                if (settled_since_us_ == 0)
                    settled_since_us_ = now_us;
                if (now_us - settled_since_us_ >= kSettleUs && remaining_m_ > kToleranceM) {
                    fault_ = "goal_overshoot";
                    return false;
                }
                if (now_us - settled_since_us_ >= kSettleUs)
                    state_ = NavigateState::kCompleted;
            } else {
                settled_since_us_ = 0;
            }
            return true;
        }
        settled_since_us_ = 0;
        if (std::abs(heading_error_rad_) > kMaxDrivingErrorRad) {
            fault_ = "heading_deviation";
            return false;
        }
        const double dt = std::clamp((now_us - last_update_us_) / 1000000.0, 0.0, 0.1);
        last_update_us_ = now_us;
        const double minimum_mps = limits_.min_speed_percent * limits_.metres_per_second_per_percent;
        const double cruise_mps = limits_.max_speed_percent * limits_.metres_per_second_per_percent;
        const double desired_speed = std::clamp(
            std::sqrt(2.0 * limits_.deceleration_mps2 * std::max(0.0, remaining_m_ - kToleranceM)),
            minimum_mps, cruise_mps);
        speed_mps_ += std::clamp(desired_speed - speed_mps_, -limits_.deceleration_mps2 * dt,
                                limits_.acceleration_mps2 * dt);
        speed_percent_ = std::clamp(
            static_cast<int>(std::lround(speed_mps_ / limits_.metres_per_second_per_percent)),
            limits_.min_speed_percent, limits_.max_speed_percent);
        return true;
    }

    void BeginDrive(int64_t now_us) {
        state_ = NavigateState::kDriving;
        speed_percent_ = limits_.min_speed_percent;
        speed_mps_ = speed_percent_ * limits_.metres_per_second_per_percent;
        last_update_us_ = now_us;
    }
    void Finish(NavigateState state, const char* fault = "none") {
        state_ = state;
        fault_ = fault;
        speed_percent_ = 0;
        speed_mps_ = 0;
    }
    bool Active() const {
        return state_ == NavigateState::kTurning || state_ == NavigateState::kDriving;
    }
    bool Turning() const { return state_ == NavigateState::kTurning; }
    bool Completed() const { return state_ == NavigateState::kCompleted; }
    bool Expired(int64_t now_us) const {
        return Active() && now_us >= deadline_us_;
    }
    Motion direction() const {
        return Turning() ? (heading_error_rad_ >= 0 ? Motion::kLeft : Motion::kRight)
                         : Motion::kForward;
    }
    int speed_percent() const { return speed_percent_; }
    double heading_error_rad() const { return heading_error_rad_; }
    const char* fault() const { return fault_; }
    NavigateSnapshot Snapshot(int64_t now_us) const {
        const char* state = "idle";
        switch (state_) {
            case NavigateState::kTurning: state = "turning"; break;
            case NavigateState::kDriving: state = "driving"; break;
            case NavigateState::kCompleted: state = "completed"; break;
            case NavigateState::kInterrupted: state = "interrupted"; break;
            case NavigateState::kFault: state = "fault"; break;
            default: break;
        }
        const char* owner = "none";
        if (state_ != NavigateState::kIdle)
            owner = request_.owner == NavigateOwner::kWeb ? "web" : "voice";
        const int remaining_ms =
            Active() ? static_cast<int>(std::max<int64_t>(0, deadline_us_ - now_us) / 1000) : 0;
        return {state, owner, fault_, request_.x_m, request_.y_m, remaining_m_,
                heading_error_rad_, speed_percent_, remaining_ms};
    }

private:
    static constexpr double kToleranceM = 0.03;
    static constexpr int64_t kTurnTimeoutUs = 10000000;
    static constexpr double kMaxDrivingErrorRad = 45.0 * std::numbers::pi / 180.0;
    static constexpr double kSettleRpm = 3;
    static constexpr int64_t kSettleUs = 300000;
    static bool ValidPose(const OdometrySnapshot& pose) {
        return pose.valid && std::isfinite(pose.x_m) && std::isfinite(pose.y_m) &&
               std::isfinite(pose.theta_rad);
    }
    double BearingError(const OdometrySnapshot& pose) const {
        return std::remainder(std::atan2(request_.y_m - pose.y_m, request_.x_m - pose.x_m) -
                                  pose.theta_rad,
                              2 * std::numbers::pi);
    }

    NavigateRequest request_;
    NavigateState state_ = NavigateState::kIdle;
    DistanceDriveLimits limits_;
    double speed_mps_ = 0;
    double remaining_m_ = 0;
    double heading_error_rad_ = 0;
    int speed_percent_ = 0;
    bool braking_ = false;
    int64_t started_us_ = 0;
    int64_t deadline_us_ = 0;
    int64_t last_update_us_ = 0;
    int64_t settled_since_us_ = 0;
    const char* fault_ = "none";
};

}  // namespace ruby_robot
