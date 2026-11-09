#pragma once

#include <cstdint>
#include <variant>

#include "motion_goal_command.h"
#include "wheel_command.h"
#include "../domain/navigator.h"

namespace ruby_robot {

enum class CommandMode { kIdle, kManual, kAngleTurn, kDistanceDrive, kNavigate };
using CommandRequest = std::variant<AngleTurnRequest, DistanceDriveRequest, NavigateRequest>;

struct CommandRequirements {
    bool wifi;
    bool speed_pi;
    bool heading;
    bool odometry;
};

struct CommandSnapshot {
    const char* mode;
    const char* owner;
    AngleTurnSnapshot angle_turn;
    DistanceDriveSnapshot distance_drive;
    NavigateSnapshot navigate;
};

// Serialized by L298nController's mutex. Chooses manual control or one motion goal.
class CommandInterface {
public:
    bool BeginManual();
    bool SubmitManual(WheelCommand wheels);
    void ReleaseManual();

    static bool Valid(const CommandRequest& request);
    static CommandRequirements Requirements(const CommandRequest& request);
    bool Start(const CommandRequest& request, int64_t now_us, const OdometrySnapshot& pose = {},
               const DistanceDriveLimits& limits = {});
    bool UpdateMotion(const OdometrySnapshot& pose, double left_rpm, double right_rpm,
                      int64_t now_us);
    bool Active() const { return mode_ != CommandMode::kIdle && mode_ != CommandMode::kManual; }
    Motion direction() const;
    int speed_percent() const;
    bool IsAngleTurn() const;
    bool IsNavigationTurn() const { return mode_ == CommandMode::kNavigate && navigator_.Turning(); }
    bool IsNavigation() const { return mode_ == CommandMode::kNavigate; }
    double NavigationHeadingErrorRad() const { return navigator_.heading_error_rad(); }
    void BeginNavigationDrive(int64_t now_us) { navigator_.BeginDrive(now_us); }
    int angle_degrees() const;
    bool Expired(int64_t now_us) const;
    bool CompletionReady() const;
    void Complete(int64_t now_us);
    void Fail(int64_t now_us, const char* fault = "control_error");
    const char* fault() const;
    void StopMotion(int64_t now_us);

    CommandMode mode() const { return mode_; }
    CommandMode last_mode() const { return last_mode_; }
    WheelCommand wheels() const;
    CommandSnapshot Snapshot(int64_t now_us) const;

    static const char* ModeName(CommandMode mode);

private:
    CommandMode mode_ = CommandMode::kIdle;
    CommandMode last_mode_ = CommandMode::kIdle;
    WheelCommand manual_wheels_;
    MotionGoalCommand goal_;
    Navigator navigator_;
};

}  // namespace ruby_robot
