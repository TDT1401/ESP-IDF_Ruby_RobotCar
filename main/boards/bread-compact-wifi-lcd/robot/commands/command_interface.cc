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

bool CommandInterface::Valid(const CommandRequest& request) {
    if (const auto* drive = std::get_if<TimeDriveRequest>(&request))
        return TimeDriveCommand::Valid(*drive);
    return false;
}

CommandRequirements CommandInterface::Requirements(const CommandRequest& request) {
    if (const auto* drive = std::get_if<TimeDriveRequest>(&request))
        return {TimeDriveCommand::kRequiresWifi, TimeDriveCommand::kRequiresSpeedPi,
                drive->direction == Motion::kForward || drive->direction == Motion::kBackward ||
                    drive->angle_degrees > 0};
    return {true, true, false};
}

bool CommandInterface::Start(const CommandRequest& request, int64_t now_us) {
    if (mode_ != CommandMode::kIdle || !Valid(request))
        return false;
    if (const auto* drive = std::get_if<TimeDriveRequest>(&request)) {
        if (!time_drive_.Start(*drive, now_us))
            return false;
        mode_ = CommandMode::kTimeDrive;
        return true;
    }
    return false;
}

Motion CommandInterface::direction() const {
    return mode_ == CommandMode::kTimeDrive ? time_drive_.direction() : Motion::kStop;
}

int CommandInterface::speed_percent() const {
    return mode_ == CommandMode::kTimeDrive ? time_drive_.speed_percent() : 0;
}

bool CommandInterface::IsAngleTurn() const {
    return mode_ == CommandMode::kTimeDrive && time_drive_.angle_degrees() > 0;
}

int CommandInterface::angle_degrees() const {
    return mode_ == CommandMode::kTimeDrive ? time_drive_.angle_degrees() : 0;
}

bool CommandInterface::Expired(int64_t now_us) const {
    return mode_ == CommandMode::kTimeDrive && time_drive_.Expired(now_us);
}

bool CommandInterface::CompletionReady() const {
    return mode_ == CommandMode::kTimeDrive && time_drive_.CompletionReady();
}

void CommandInterface::Complete(int64_t now_us) {
    mode_ = CommandMode::kIdle;
    time_drive_.Complete(now_us);
}

void CommandInterface::Fail(int64_t now_us) {
    mode_ = CommandMode::kIdle;
    time_drive_.Fail(now_us);
}

void CommandInterface::RecordSpeedSample(double left_target_rpm, double right_target_rpm,
                                         double left_measured_rpm, double right_measured_rpm,
                                         int left_pwm_percent, int right_pwm_percent) {
    if (mode_ == CommandMode::kTimeDrive)
        time_drive_.RecordSample(left_target_rpm, right_target_rpm, left_measured_rpm,
                                 right_measured_rpm, left_pwm_percent, right_pwm_percent);
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

CommandSnapshot CommandInterface::Snapshot(int64_t now_us) const {
    const char* owner = mode_ == CommandMode::kTimeDrive
                            ? TimeDriveCommand::OwnerName(time_drive_.owner())
                            : mode_ == CommandMode::kManual ? "web" : "none";
    return {ModeName(mode_), owner, time_drive_.Snapshot(now_us)};
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
