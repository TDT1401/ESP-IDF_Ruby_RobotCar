#pragma once

#include <cstdint>
#include <variant>

#include "distance_drive_command.h"
#include "manual_command.h"
#include "time_drive_command.h"

namespace ruby_robot {

enum class CommandMode { kIdle, kManual, kTimeDrive, kDistanceDrive };
using CommandRequest = std::variant<TimeDriveRequest, DistanceDriveRequest>;

struct CommandRequirements {
    bool wifi;
    bool speed_pi;
    bool heading;
    bool odometry;
};

struct CommandSnapshot {
    const char* mode;
    const char* owner;
    TimeDriveSnapshot time_drive;
    DistanceDriveSnapshot distance_drive;
};

// Serialized by L298nController's mutex. Chooses one mode at a time;
// each mode owns its targets and state in its own command class.
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
    int angle_degrees() const;
    bool Expired(int64_t now_us) const;
    bool CompletionReady() const;
    void Complete(int64_t now_us);
    void Fail(int64_t now_us, const char* fault = "control_error");
    const char* fault() const;
    void RecordSpeedSample(double left_target_rpm, double right_target_rpm,
                           double left_measured_rpm, double right_measured_rpm,
                           int left_pwm_percent, int right_pwm_percent);
    void StopMotion(int64_t now_us);

    CommandMode mode() const { return mode_; }
    CommandMode last_mode() const { return last_mode_; }
    WheelCommand wheels() const;
    CommandSnapshot Snapshot(int64_t now_us) const;

    static const char* ModeName(CommandMode mode);

private:
    CommandMode mode_ = CommandMode::kIdle;
    CommandMode last_mode_ = CommandMode::kIdle;
    ManualCommand manual_;
    TimeDriveCommand time_drive_;
    DistanceDriveCommand distance_drive_;
};

}  // namespace ruby_robot
