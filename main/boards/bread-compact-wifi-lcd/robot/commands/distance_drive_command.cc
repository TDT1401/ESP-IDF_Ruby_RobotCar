#include "distance_drive_command.h"

#include <algorithm>
#include <cmath>

namespace ruby_robot {
namespace {
bool ValidPose(const OdometrySnapshot& pose) {
    return pose.valid && std::isfinite(pose.x_m) && std::isfinite(pose.y_m) &&
           std::isfinite(pose.theta_rad) && std::isfinite(pose.centre_m);
}
}  // namespace

bool DistanceDriveCommand::Valid(const DistanceDriveRequest& request) {
    return ((request.distance_cm >= kMinDistanceCm && request.distance_cm <= kMaxDistanceCm) ||
            (request.distance_cm <= -kMinDistanceCm && request.distance_cm >= -kMaxDistanceCm)) &&
           request.speed_percent >= 10 && request.speed_percent <= 100 &&
           request.timeout_seconds >= 1 && request.timeout_seconds <= kMaxTimeoutSeconds &&
           (request.owner == DistanceDriveOwner::kWeb ||
            request.owner == DistanceDriveOwner::kVoice ||
            request.owner == DistanceDriveOwner::kTelegram);
}

bool DistanceDriveCommand::Start(const DistanceDriveRequest& request,
                                 const DistanceDriveLimits& limits, const OdometrySnapshot& pose,
                                 int64_t now_us) {
    if (!Valid(request) || !ValidPose(pose) ||
        !std::isfinite(limits.metres_per_second_per_percent) ||
        limits.metres_per_second_per_percent <= 0 || !std::isfinite(limits.acceleration_mps2) ||
        limits.acceleration_mps2 <= 0 || !std::isfinite(limits.deceleration_mps2) ||
        limits.deceleration_mps2 <= 0 || !std::isfinite(limits.tolerance_m) ||
        limits.tolerance_m <= 0 || limits.tolerance_m >= kMinDistanceCm / 100.0 ||
        limits.min_speed_percent < 1 || limits.min_speed_percent > 100 ||
        !std::isfinite(limits.settle_rpm) || limits.settle_rpm < 0 || limits.settle_us <= 0)
        return false;
    request_ = request;
    limits_ = limits;
    state_ = DistanceDriveState::kRunning;
    start_centre_m_ = pose.centre_m;
    const double target_m = request.distance_cm / 100.0;
    goal_x_m_ = pose.x_m + target_m * std::cos(pose.theta_rad);
    goal_y_m_ = pose.y_m + target_m * std::sin(pose.theta_rad);
    progress_m_ = 0;
    position_error_m_ = std::abs(target_m);
    speed_mps_ = std::min(request.speed_percent, limits.min_speed_percent) *
                 limits.metres_per_second_per_percent;
    braking_ = completion_ready_ = false;
    started_us_ = last_update_us_ = now_us;
    settled_since_us_ = ended_us_ = 0;
    fault_ = "none";
    return true;
}

bool DistanceDriveCommand::Update(const OdometrySnapshot& pose, double left_rpm, double right_rpm,
                                  int64_t now_us) {
    if (state_ != DistanceDriveState::kRunning)
        return true;
    if (!ValidPose(pose)) {
        fault_ = pose.valid ? "invalid_pose" : pose.fault;
        return false;
    }
    if (!std::isfinite(left_rpm) || !std::isfinite(right_rpm) || left_rpm < 0 || right_rpm < 0) {
        fault_ = "encoder_error";
        return false;
    }
    progress_m_ = pose.centre_m - start_centre_m_;
    position_error_m_ = std::hypot(goal_x_m_ - pose.x_m, goal_y_m_ - pose.y_m);
    const double sign = request_.distance_cm < 0 ? -1.0 : 1.0;
    const double remaining_m = std::abs(request_.distance_cm / 100.0) - sign * progress_m_;
    const double dt_s = std::clamp((now_us - last_update_us_) / 1000000.0, 0.0, 0.1);
    last_update_us_ = now_us;
    braking_ = braking_ || remaining_m <= limits_.tolerance_m;
    if (braking_) {
        speed_mps_ = 0;
        // Keep zero targets while counting coast pulses; never drive backwards to
        // hide an overshoot. Require both wheels to settle before reporting success.
        if (left_rpm <= limits_.settle_rpm && right_rpm <= limits_.settle_rpm) {
            if (settled_since_us_ == 0)
                settled_since_us_ = now_us;
            if (now_us - settled_since_us_ >= limits_.settle_us) {
                if (std::abs(remaining_m) > limits_.tolerance_m) {
                    fault_ = "distance_overshoot";
                    return false;
                }
                completion_ready_ = true;
            }
        } else {
            settled_since_us_ = 0;
        }
        return true;
    }
    const double cruise_mps = request_.speed_percent * limits_.metres_per_second_per_percent;
    const double minimum_mps = std::min(request_.speed_percent, limits_.min_speed_percent) *
                               limits_.metres_per_second_per_percent;
    const double desired_mps =
        std::clamp(std::sqrt(2.0 * limits_.deceleration_mps2 * (remaining_m - limits_.tolerance_m)),
                   minimum_mps, cruise_mps);
    speed_mps_ += std::clamp(desired_mps - speed_mps_, -limits_.deceleration_mps2 * dt_s,
                             limits_.acceleration_mps2 * dt_s);
    return true;
}

bool DistanceDriveCommand::Expired(int64_t now_us) const {
    return state_ == DistanceDriveState::kRunning &&
           now_us - started_us_ >= request_.timeout_seconds * 1000000LL;
}

void DistanceDriveCommand::Finish(DistanceDriveState state, int64_t now_us, const char* fault) {
    state_ = state;
    ended_us_ = now_us;
    speed_mps_ = 0;
    fault_ = fault;
}

int DistanceDriveCommand::speed_percent() const {
    if (state_ != DistanceDriveState::kRunning || braking_)
        return 0;
    return std::clamp(
        static_cast<int>(std::lround(speed_mps_ / limits_.metres_per_second_per_percent)), 0,
        request_.speed_percent);
}

WheelCommand DistanceDriveCommand::wheels() const {
    const int speed = speed_percent() * (request_.distance_cm < 0 ? -1 : 1);
    return {speed, speed};
}

DistanceDriveSnapshot DistanceDriveCommand::Snapshot(int64_t now_us) const {
    const char* state = "idle";
    switch (state_) {
        case DistanceDriveState::kRunning:
            state = "moving";
            break;
        case DistanceDriveState::kCompleted:
            state = "completed";
            break;
        case DistanceDriveState::kInterrupted:
            state = "interrupted";
            break;
        case DistanceDriveState::kFault:
            state = "fault";
            break;
        default:
            break;
    }
    const int64_t elapsed =
        state_ == DistanceDriveState::kIdle
            ? 0
            : (state_ == DistanceDriveState::kRunning ? now_us : ended_us_) - started_us_;
    const int elapsed_ms =
        static_cast<int>(std::clamp<int64_t>(elapsed / 1000, 0, request_.timeout_seconds * 1000));
    return {
        state,
        OwnerName(request_.owner),
        fault_,
        request_.speed_percent,
        request_.timeout_seconds,
        elapsed_ms,
        state_ == DistanceDriveState::kRunning ? request_.timeout_seconds * 1000 - elapsed_ms : 0,
        request_.distance_cm / 100.0,
        progress_m_,
        request_.distance_cm / 100.0 - progress_m_,
        position_error_m_};
}

const char* DistanceDriveCommand::OwnerName(DistanceDriveOwner owner) {
    switch (owner) {
        case DistanceDriveOwner::kVoice:
            return "voice";
        case DistanceDriveOwner::kTelegram:
            return "telegram";
        default:
            return "web";
    }
}

}  // namespace ruby_robot
