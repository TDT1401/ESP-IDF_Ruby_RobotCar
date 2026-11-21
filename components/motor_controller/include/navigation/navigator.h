#pragma once

#include <cstdint>
#include <numbers>

#include "control/motion_policy.h"
#include "distance/distance_drive.h"
#include "odometry/odometry.h"

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

    static bool Valid(const NavigateRequest& request);
    bool Start(const NavigateRequest& request, const OdometrySnapshot& pose,
               const DistanceDriveLimits& limits, int64_t now_us);
    bool Update(const OdometrySnapshot& pose, double left_rpm, double right_rpm, int64_t now_us);
    void BeginDrive(int64_t now_us);
    void Finish(NavigateState state, const char* fault = "none");
    bool Active() const;
    bool Turning() const;
    bool Completed() const;
    bool Expired(int64_t now_us) const;
    Motion direction() const;
    int speed_percent() const;
    double heading_error_rad() const;
    const char* fault() const { return fault_; }
    NavigateSnapshot Snapshot(int64_t now_us) const;

private:
    static constexpr double kToleranceM = 0.03;
    static constexpr int64_t kTurnTimeoutUs = 10000000;
    static constexpr double kMaxDrivingErrorRad = 45.0 * std::numbers::pi / 180.0;
    static constexpr double kSettleRpm = 3;
    static constexpr int64_t kSettleUs = 300000;
    static bool ValidPose(const OdometrySnapshot& pose);
    double BearingError(const OdometrySnapshot& pose) const;

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
