#pragma once

#include <cstdint>

#include "manual_command.h"
#include "time_drive_command.h"

namespace ruby_robot {

enum class CommandMode { kIdle, kManual, kTimeDrive };

// Serialized by L298nController's mutex. Chooses one mode at a time;
// each mode owns its targets and state in its own command class.
class CommandInterface {
public:
    bool BeginManual();
    bool SubmitManual(WheelCommand wheels);
    void ReleaseManual();

    bool BeginTimeDrive(Motion direction, int speed_percent, int duration_seconds,
                        TimeDriveOwner owner, int64_t now_us);
    bool TimeDriveExpired(int64_t now_us) const;
    void CompleteTimeDrive(int64_t now_us);
    void FailTimeDrive(int64_t now_us);
    void StopMotion(int64_t now_us);

    CommandMode mode() const { return mode_; }
    WheelCommand wheels() const;
    const TimeDriveCommand& time_drive() const { return time_drive_; }
    TimeDriveCommand& time_drive() { return time_drive_; }

    static const char* ModeName(CommandMode mode);

private:
    CommandMode mode_ = CommandMode::kIdle;
    ManualCommand manual_;
    TimeDriveCommand time_drive_;
};

}  // namespace ruby_robot
