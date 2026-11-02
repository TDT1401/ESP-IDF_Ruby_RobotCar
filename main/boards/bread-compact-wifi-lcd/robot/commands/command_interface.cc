#include "command_interface.h"

namespace ruby_robot {

bool CommandInterface::BeginManual() {
    if (mode_ != CommandMode::kIdle)
        return false;
    manual_.Clear();
    mode_ = CommandMode::kManual;
    return true;
}

bool CommandInterface::SubmitManual(WheelCommand wheels) {
    return mode_ == CommandMode::kManual && manual_.Submit(wheels);
}

void CommandInterface::ReleaseManual() {
    if (mode_ == CommandMode::kManual) {
        manual_.Clear();
        mode_ = CommandMode::kIdle;
    }
}

bool CommandInterface::BeginTimeDrive(Motion direction, int speed_percent, int duration_seconds,
                                      TimeDriveOwner owner, int64_t now_us) {
    if (mode_ != CommandMode::kIdle ||
        !time_drive_.Start(direction, speed_percent, duration_seconds, owner, now_us))
        return false;
    mode_ = CommandMode::kTimeDrive;
    return true;
}

bool CommandInterface::TimeDriveExpired(int64_t now_us) const {
    return mode_ == CommandMode::kTimeDrive && time_drive_.Expired(now_us);
}

void CommandInterface::CompleteTimeDrive(int64_t now_us) {
    mode_ = CommandMode::kIdle;
    time_drive_.Complete(now_us);
}

void CommandInterface::FailTimeDrive(int64_t now_us) {
    mode_ = CommandMode::kIdle;
    time_drive_.Fail(now_us);
}

void CommandInterface::StopMotion(int64_t now_us) {
    if (mode_ == CommandMode::kTimeDrive) {
        mode_ = CommandMode::kIdle;
        time_drive_.Interrupt(now_us);
    } else if (mode_ == CommandMode::kManual) {
        manual_.Clear();
    }
}

WheelCommand CommandInterface::wheels() const {
    if (mode_ == CommandMode::kManual)
        return manual_.wheels();
    if (mode_ == CommandMode::kTimeDrive)
        return time_drive_.wheels();
    return {};
}

const char* CommandInterface::ModeName(CommandMode mode) {
    switch (mode) {
        case CommandMode::kManual:
            return "manual";
        case CommandMode::kTimeDrive:
            return "time_drive";
        default:
            return "idle";
    }
}

}  // namespace ruby_robot
