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
    if (const auto* turn = std::get_if<AngleTurnRequest>(&request))
        return MotionGoalCommand::Valid(*turn);
    if (const auto* drive = std::get_if<DistanceDriveRequest>(&request))
        return MotionGoalCommand::Valid(*drive);
    return false;
}

CommandRequirements CommandInterface::Requirements(const CommandRequest& request) {
    if (std::holds_alternative<AngleTurnRequest>(request))
        return {true, true, true, false};
    return {true, true, true, true};
}

bool CommandInterface::Start(const CommandRequest& request, int64_t now_us,
                             const OdometrySnapshot& pose, const DistanceDriveLimits& limits) {
    if (mode_ != CommandMode::kIdle || !Valid(request))
        return false;
    if (const auto* turn = std::get_if<AngleTurnRequest>(&request)) {
        if (!goal_.Start(*turn, now_us))
            return false;
        last_mode_ = mode_ = CommandMode::kAngleTurn;
        return true;
    }
    if (const auto* drive = std::get_if<DistanceDriveRequest>(&request)) {
        if (!goal_.Start(*drive, limits, pose, now_us))
            return false;
        last_mode_ = mode_ = CommandMode::kDistanceDrive;
        return true;
    }
    return false;
}

Motion CommandInterface::direction() const {
    if (mode_ == CommandMode::kDistanceDrive)
        return goal_.direction();
    return mode_ == CommandMode::kAngleTurn ? goal_.turn_direction() : Motion::kStop;
}

int CommandInterface::speed_percent() const {
    if (mode_ == CommandMode::kDistanceDrive)
        return goal_.speed_percent();
    return mode_ == CommandMode::kAngleTurn ? goal_.turn_speed_percent() : 0;
}

bool CommandInterface::IsAngleTurn() const { return mode_ == CommandMode::kAngleTurn; }

int CommandInterface::angle_degrees() const {
    return mode_ == CommandMode::kAngleTurn
               ? (goal_.angle_degrees() < 0 ? -goal_.angle_degrees() : goal_.angle_degrees())
               : 0;
}

bool CommandInterface::Expired(int64_t now_us) const {
    if (mode_ == CommandMode::kDistanceDrive)
        return goal_.Expired(now_us);
    return mode_ == CommandMode::kAngleTurn && goal_.Expired(now_us);
}

bool CommandInterface::CompletionReady() const {
    if (mode_ == CommandMode::kDistanceDrive)
        return goal_.CompletionReady();
    return false;
}

void CommandInterface::Complete(int64_t now_us) {
    if (mode_ == CommandMode::kAngleTurn)
        goal_.Finish(AngleTurnState::kCompleted);
    else if (mode_ == CommandMode::kDistanceDrive)
        goal_.Finish(DistanceDriveState::kCompleted, now_us);
    mode_ = CommandMode::kIdle;
}

void CommandInterface::Fail(int64_t now_us, const char* fault) {
    if (mode_ == CommandMode::kAngleTurn)
        goal_.Finish(AngleTurnState::kFault);
    else if (mode_ == CommandMode::kDistanceDrive)
        goal_.Finish(DistanceDriveState::kFault, now_us, fault);
    mode_ = CommandMode::kIdle;
}

bool CommandInterface::UpdateMotion(const OdometrySnapshot& pose, double left_rpm, double right_rpm,
                                    int64_t now_us) {
    return mode_ != CommandMode::kDistanceDrive || goal_.Update(pose, left_rpm, right_rpm, now_us);
}

const char* CommandInterface::fault() const {
    return mode_ == CommandMode::kDistanceDrive ? goal_.fault() : "none";
}

void CommandInterface::StopMotion(int64_t now_us) {
    if (mode_ == CommandMode::kAngleTurn) {
        mode_ = CommandMode::kIdle;
        goal_.Finish(AngleTurnState::kInterrupted);
    } else if (mode_ == CommandMode::kDistanceDrive) {
        goal_.Finish(DistanceDriveState::kInterrupted, now_us);
        mode_ = CommandMode::kIdle;
    } else if (mode_ == CommandMode::kManual) {
        manual_.Clear();
    }
}

WheelCommand CommandInterface::wheels() const {
    if (mode_ == CommandMode::kManual)
        return manual_.wheels();
    if (mode_ == CommandMode::kAngleTurn)
        return goal_.turn_wheels();
    if (mode_ == CommandMode::kDistanceDrive)
        return goal_.wheels();
    return {};
}

CommandSnapshot CommandInterface::Snapshot(int64_t now_us) const {
    const auto distance = goal_.Snapshot(now_us);
    const auto turn = goal_.TurnSnapshot();
    const char* owner = mode_ == CommandMode::kDistanceDrive ? distance.owner
                        : mode_ == CommandMode::kAngleTurn   ? turn.owner
                        : mode_ == CommandMode::kManual      ? "web"
                                                             : "none";
    return {ModeName(mode_), owner, turn, distance};
}

const char* CommandInterface::ModeName(CommandMode mode) {
    switch (mode) {
        case CommandMode::kManual:
            return "manual";
        case CommandMode::kAngleTurn:
            return "angle_turn";
        case CommandMode::kDistanceDrive:
            return "distance_drive";
        default:
            return "idle";
    }
}

}  // namespace ruby_robot
