#include "time_drive_command.h"

#include <algorithm>
#include <cmath>

namespace ruby_robot {

bool TimeDriveCommand::Start(Motion direction, int speed_percent, int duration_seconds,
                             TimeDriveOwner owner, int64_t now_us) {
    if ((direction != Motion::kForward && direction != Motion::kBackward &&
         direction != Motion::kLeft && direction != Motion::kRight) ||
        speed_percent < kMinSpeedPercent || speed_percent > 100 ||
        duration_seconds < kMinSeconds || duration_seconds > kMaxSeconds)
        return false;
    const int left_sign = direction == Motion::kBackward || direction == Motion::kLeft ? -1 : 1;
    const int right_sign = direction == Motion::kBackward || direction == Motion::kRight ? -1 : 1;
    wheels_ = {left_sign * speed_percent, right_sign * speed_percent};
    state_ = TimeDriveState::kRunning;
    owner_ = owner;
    direction_ = direction;
    speed_percent_ = speed_percent;
    duration_seconds_ = duration_seconds;
    started_us_ = now_us;
    deadline_us_ = now_us + duration_seconds * 1000000LL;
    ended_us_ = 0;
    totals_ = {};
    left_rpm_squares_ = 0;
    right_rpm_squares_ = 0;
    return true;
}

bool TimeDriveCommand::Expired(int64_t now_us) const {
    return state_ == TimeDriveState::kRunning && now_us >= deadline_us_;
}

void TimeDriveCommand::Complete(int64_t now_us) {
    wheels_ = {};
    state_ = TimeDriveState::kCompleted;
    ended_us_ = std::min(now_us, deadline_us_);
}

void TimeDriveCommand::Fail(int64_t now_us) {
    wheels_ = {};
    state_ = TimeDriveState::kFault;
    ended_us_ = now_us;
}

void TimeDriveCommand::Interrupt(int64_t now_us) {
    wheels_ = {};
    state_ = TimeDriveState::kInterrupted;
    ended_us_ = now_us;
}

void TimeDriveCommand::RecordSample(double left_target_rpm, double right_target_rpm,
                                    double left_measured_rpm, double right_measured_rpm,
                                    int left_pwm_percent, int right_pwm_percent) {
    if (state_ != TimeDriveState::kRunning)
        return;
    ++totals_.samples;
    totals_.average_left_rpm += left_measured_rpm;
    totals_.average_right_rpm += right_measured_rpm;
    left_rpm_squares_ += left_measured_rpm * left_measured_rpm;
    right_rpm_squares_ += right_measured_rpm * right_measured_rpm;
    totals_.average_left_error_rpm += std::abs(left_target_rpm - left_measured_rpm);
    totals_.average_right_error_rpm += std::abs(right_target_rpm - right_measured_rpm);
    totals_.average_left_pwm_percent += left_pwm_percent;
    totals_.average_right_pwm_percent += right_pwm_percent;
}

TimeDriveMetrics TimeDriveCommand::GetMetrics() const {
    TimeDriveMetrics result = totals_;
    if (result.samples > 0) {
        result.average_left_rpm /= result.samples;
        result.average_right_rpm /= result.samples;
        result.stddev_left_rpm =
            std::sqrt(std::max(0.0, left_rpm_squares_ / result.samples -
                                       result.average_left_rpm * result.average_left_rpm));
        result.stddev_right_rpm =
            std::sqrt(std::max(0.0, right_rpm_squares_ / result.samples -
                                       result.average_right_rpm * result.average_right_rpm));
        result.average_left_error_rpm /= result.samples;
        result.average_right_error_rpm /= result.samples;
        result.average_left_pwm_percent /= result.samples;
        result.average_right_pwm_percent /= result.samples;
    }
    return result;
}

int TimeDriveCommand::ElapsedMs(int64_t now_us) const {
    if (state_ == TimeDriveState::kIdle)
        return 0;
    const int64_t end_us = ended_us_ > 0 ? ended_us_ : now_us;
    const int64_t elapsed_us = std::clamp<int64_t>(
        end_us - started_us_, 0, duration_seconds_ * 1000000LL);
    return static_cast<int>(elapsed_us / 1000);
}

int TimeDriveCommand::RemainingMs(int64_t now_us) const {
    if (state_ != TimeDriveState::kRunning)
        return 0;
    return static_cast<int>(std::max<int64_t>(0, deadline_us_ - now_us) / 1000);
}

const char* TimeDriveCommand::StateName(TimeDriveState state) {
    switch (state) {
        case TimeDriveState::kRunning:
            return "running";
        case TimeDriveState::kCompleted:
            return "completed";
        case TimeDriveState::kInterrupted:
            return "interrupted";
        case TimeDriveState::kFault:
            return "fault";
        default:
            return "idle";
    }
}

const char* TimeDriveCommand::OwnerName(TimeDriveOwner owner) {
    return owner == TimeDriveOwner::kVoice ? "voice" : "web";
}

}  // namespace ruby_robot
