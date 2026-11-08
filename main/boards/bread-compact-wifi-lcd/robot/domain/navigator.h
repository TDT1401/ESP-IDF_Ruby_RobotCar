#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "motion_policy.h"
#include "odometry.h"

namespace ruby_robot {

enum class NavigateOwner { kWeb, kVoice, kTelegram };
enum class NavigateState { kIdle, kTurning, kDriving, kCompleted, kInterrupted, kFault };

struct NavigateRequest {
    double x_m = 0;
    double y_m = 0;
    int speed_percent = 40;
    int timeout_seconds = 60;
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
    static bool Valid(const NavigateRequest& request) {
        return std::isfinite(request.x_m) && std::isfinite(request.y_m) &&
               std::abs(request.x_m) <= 10 && std::abs(request.y_m) <= 10 &&
               request.speed_percent >= 18 && request.speed_percent <= 70 &&
               request.timeout_seconds >= 1 && request.timeout_seconds <= 120 &&
               (request.owner == NavigateOwner::kWeb || request.owner == NavigateOwner::kVoice ||
                request.owner == NavigateOwner::kTelegram);
    }

    bool Start(const NavigateRequest& request, const OdometrySnapshot& pose, double speed_scale,
               int64_t now_us) {
        if (!Valid(request) || !ValidPose(pose) || !std::isfinite(speed_scale) || speed_scale <= 0)
            return false;
        const double distance = std::hypot(request.x_m - pose.x_m, request.y_m - pose.y_m);
        if (!std::isfinite(distance) || distance < kToleranceM)
            return false;
        request_ = request;
        speed_scale_ = speed_scale;
        started_us_ = last_update_us_ = now_us;
        settled_since_us_ = 0;
        braking_ = false;
        remaining_m_ = distance;
        heading_error_rad_ = BearingError(pose);
        state_ = std::abs(heading_error_rad_) > kHeadingToleranceRad ? NavigateState::kTurning
                                                                     : NavigateState::kDriving;
        speed_percent_ = state_ == NavigateState::kTurning ? kTurnSpeedPercent : kMinSpeedPercent;
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
        if (state_ == NavigateState::kTurning)
            return true;
        braking_ = braking_ || remaining_m_ <= kToleranceM;
        if (braking_) {
            speed_percent_ = 0;
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
        const double desired_speed = std::clamp(
            std::sqrt(2.0 * kDecelerationMps2 * std::max(0.0, remaining_m_ - kToleranceM)),
            kMinSpeedPercent * speed_scale_, request_.speed_percent * speed_scale_);
        const double current_speed = speed_percent_ * speed_scale_;
        speed_percent_ = std::clamp(
            static_cast<int>(std::lround((current_speed +
                                          std::clamp(desired_speed - current_speed,
                                                     -kDecelerationMps2 * dt,
                                                     kAccelerationMps2 * dt)) /
                                         speed_scale_)),
            kMinSpeedPercent, request_.speed_percent);
        return true;
    }

    void BeginDrive(int64_t now_us) {
        state_ = NavigateState::kDriving;
        speed_percent_ = kMinSpeedPercent;
        last_update_us_ = now_us;
    }
    void Finish(NavigateState state, const char* fault = "none") {
        state_ = state;
        fault_ = fault;
        speed_percent_ = 0;
    }
    bool Active() const {
        return state_ == NavigateState::kTurning || state_ == NavigateState::kDriving;
    }
    bool Turning() const { return state_ == NavigateState::kTurning; }
    bool Completed() const { return state_ == NavigateState::kCompleted; }
    bool Expired(int64_t now_us) const {
        return Active() && now_us - started_us_ >= request_.timeout_seconds * 1000000LL;
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
        const char* owner = state_ == NavigateState::kIdle          ? "none"
                            : request_.owner == NavigateOwner::kWeb ? "web"
                            : request_.owner == NavigateOwner::kTelegram
                                ? "telegram"
                                : "voice";
        const int remaining_ms = Active()
                                     ? static_cast<int>(std::max<int64_t>(
                                           0, request_.timeout_seconds * 1000LL -
                                                  (now_us - started_us_) / 1000))
                                     : 0;
        return {state, owner, fault_, request_.x_m, request_.y_m, remaining_m_,
                heading_error_rad_, speed_percent_, remaining_ms};
    }

private:
    static constexpr double kToleranceM = 0.03;
    static constexpr double kHeadingToleranceRad = 3.0 * std::numbers::pi / 180.0;
    static constexpr double kMaxDrivingErrorRad = 45.0 * std::numbers::pi / 180.0;
    static constexpr double kAccelerationMps2 = 0.3;
    static constexpr double kDecelerationMps2 = 0.3;
    static constexpr double kSettleRpm = 3;
    static constexpr int64_t kSettleUs = 300000;
    static constexpr int kTurnSpeedPercent = 40;
    static constexpr int kMinSpeedPercent = 18;
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
    double speed_scale_ = 0;
    double remaining_m_ = 0;
    double heading_error_rad_ = 0;
    int speed_percent_ = 0;
    bool braking_ = false;
    int64_t started_us_ = 0;
    int64_t last_update_us_ = 0;
    int64_t settled_since_us_ = 0;
    const char* fault_ = "none";
};

}  // namespace ruby_robot
