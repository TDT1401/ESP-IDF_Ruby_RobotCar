#include "navigation/navigator.h"

#include <algorithm>
#include <cmath>

namespace ruby_robot {

bool Navigator::Valid(const NavigateRequest& request) {
    return std::isfinite(request.x_m) && std::isfinite(request.y_m) &&
           std::abs(request.x_m) <= 10 && std::abs(request.y_m) <= 10 &&
           (request.owner == NavigateOwner::kWeb || request.owner == NavigateOwner::kVoice);
}

bool Navigator::Start(const NavigateRequest& request, const OdometrySnapshot& pose,
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

bool Navigator::Update(const OdometrySnapshot& pose, double left_rpm, double right_rpm, int64_t now_us) {
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

void Navigator::BeginDrive(int64_t now_us) {
    state_ = NavigateState::kDriving;
    speed_percent_ = limits_.min_speed_percent;
    speed_mps_ = speed_percent_ * limits_.metres_per_second_per_percent;
    last_update_us_ = now_us;
}

void Navigator::Finish(NavigateState state, const char* fault) {
    state_ = state;
    fault_ = fault;
    speed_percent_ = 0;
    speed_mps_ = 0;
}

bool Navigator::Active() const {
    return state_ == NavigateState::kTurning || state_ == NavigateState::kDriving;
}

bool Navigator::Turning() const { return state_ == NavigateState::kTurning; }

bool Navigator::Completed() const { return state_ == NavigateState::kCompleted; }

bool Navigator::Expired(int64_t now_us) const {
    return Active() && now_us >= deadline_us_;
}

Motion Navigator::direction() const {
    return Turning() ? (heading_error_rad_ >= 0 ? Motion::kLeft : Motion::kRight)
                     : Motion::kForward;
}

int Navigator::speed_percent() const { return speed_percent_; }

double Navigator::heading_error_rad() const { return heading_error_rad_; }

NavigateSnapshot Navigator::Snapshot(int64_t now_us) const {
    const char* state = "idle";
    switch (state_) {
        case NavigateState::kTurning:
            state = "turning";
            break;
        case NavigateState::kDriving:
            state = "driving";
            break;
        case NavigateState::kCompleted:
            state = "completed";
            break;
        case NavigateState::kInterrupted:
            state = "interrupted";
            break;
        case NavigateState::kFault:
            state = "fault";
            break;
        default:
            break;
    }
    const char* owner = "none";
    if (state_ != NavigateState::kIdle)
        owner = request_.owner == NavigateOwner::kWeb ? "web" : "voice";
    const int remaining_ms =
        Active() ? static_cast<int>(std::max<int64_t>(0, deadline_us_ - now_us) / 1000) : 0;
    return {state, owner, fault_, request_.x_m, request_.y_m, remaining_m_,
            heading_error_rad_, speed_percent_, remaining_ms};
}

bool Navigator::ValidPose(const OdometrySnapshot& pose) {
    return pose.valid && std::isfinite(pose.x_m) && std::isfinite(pose.y_m) &&
           std::isfinite(pose.theta_rad);
}

double Navigator::BearingError(const OdometrySnapshot& pose) const {
    return std::remainder(std::atan2(request_.y_m - pose.y_m, request_.x_m - pose.x_m) -
                              pose.theta_rad,
                          2 * std::numbers::pi);
}

}  // namespace ruby_robot
