#pragma once

#include <cstdint>

#include "control/motion_policy.h"
#include "odometry/odometry.h"

namespace ruby_robot {

enum class DistanceDriveOwner { kWeb, kVoice };
enum class DistanceDriveState { kIdle, kRunning, kCompleted, kInterrupted, kFault };

struct DistanceDriveRequest {
    int distance_cm = 0;  // Positive forward, negative backward.
    int timeout_seconds = 60;
    DistanceDriveOwner owner = DistanceDriveOwner::kWeb;
};

struct DistanceDriveLimits {
    double metres_per_second_per_percent = 0;
    double acceleration_mps2 = 0;
    double deceleration_mps2 = 0;
    double tolerance_m = 0;
    int min_speed_percent = 0;
    int max_speed_percent = 0;
    double settle_rpm = 0;
    int64_t settle_us = 0;
};

struct DistanceDriveSnapshot {
    const char* state;
    const char* owner;
    const char* fault;
    int speed_percent;  // Current automatic profile target, not applied PWM.
    int timeout_seconds;
    int elapsed_ms;
    int remaining_ms;
    double target_m;
    double progress_m;
    double error_m;
    double position_error_m;  // Estimated endpoint error, not independent ground truth.
};

// Distance goal and speed profile. The controller owns sensors and motor output.
class DistanceDrive {
public:
    static constexpr int kMinDistanceCm = 5;
    static constexpr int kMaxDistanceCm = 1000;
    static constexpr int kMaxTimeoutSeconds = 120;

    static bool Valid(const DistanceDriveRequest& request);
    bool Start(const DistanceDriveRequest& request, const DistanceDriveLimits& limits,
               const OdometrySnapshot& pose, int64_t now_us);
    bool Update(const OdometrySnapshot& pose, double left_rpm, double right_rpm, int64_t now_us);
    bool Expired(int64_t now_us) const;
    bool CompletionReady() const { return completion_ready_; }
    void Finish(DistanceDriveState state, int64_t now_us, const char* fault = "none");
    int speed_percent() const;
    Motion direction() const {
        return request_.distance_cm < 0 ? Motion::kBackward : Motion::kForward;
    }
    DistanceDriveSnapshot Snapshot(int64_t now_us) const;
    const char* fault() const { return fault_; }

private:
    DistanceDriveRequest request_;
    DistanceDriveLimits limits_;
    DistanceDriveState state_ = DistanceDriveState::kIdle;
    double start_centre_m_ = 0;
    double goal_x_m_ = 0;
    double goal_y_m_ = 0;
    double progress_m_ = 0;
    double position_error_m_ = 0;
    double speed_mps_ = 0;
    bool braking_ = false;
    bool completion_ready_ = false;
    int64_t started_us_ = 0;
    int64_t last_update_us_ = 0;
    int64_t settled_since_us_ = 0;
    int64_t ended_us_ = 0;
    const char* fault_ = "none";
};

}  // namespace ruby_robot
