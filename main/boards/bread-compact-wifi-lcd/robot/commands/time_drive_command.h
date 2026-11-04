#pragma once

#include <cstdint>

#include "../domain/motion_policy.h"
#include "wheel_command.h"

namespace ruby_robot {

struct TimeDriveMetrics {
    int samples = 0;
    double average_left_rpm = 0;
    double average_right_rpm = 0;
    double average_left_error_rpm = 0;
    double average_right_error_rpm = 0;
    double average_left_pwm_percent = 0;
    double average_right_pwm_percent = 0;
    double stddev_left_rpm = 0;
    double stddev_right_rpm = 0;
};

enum class TimeDriveState { kIdle, kRunning, kCompleted, kInterrupted, kFault };
enum class TimeDriveOwner { kWeb, kVoice };

struct TimeDriveRequest {
    Motion direction = Motion::kStop;
    int speed_percent = 0;  // For turns, 0 asks the controller to select the speed limit.
    int duration_seconds = 0;
    TimeDriveOwner owner = TimeDriveOwner::kWeb;
    int angle_degrees = 0;  // Required for turns; duration is then a safety timeout.
};

struct TimeDriveSnapshot {
    const char* state;
    const char* owner;
    const char* direction;
    int speed_percent;
    int duration_seconds;
    int angle_degrees;  // Signed: positive left, negative right.
    int elapsed_ms;
    int remaining_ms;
    TimeDriveMetrics metrics;
};

// Owns one timed command and its Speed PI measurements; motor output stays in the controller.
class TimeDriveCommand {
public:
    static constexpr int kMinSeconds = 1;
    static constexpr int kMaxSeconds = 30;
    static constexpr int kMinSpeedPercent = 10;
    static constexpr bool kRequiresWifi = true;
    static constexpr bool kRequiresSpeedPi = true;

    static bool Valid(const TimeDriveRequest& request);

    bool Start(const TimeDriveRequest& request, int64_t now_us);
    bool Expired(int64_t now_us) const;
    bool CompletionReady() const { return totals_.samples > 0; }
    void Complete(int64_t now_us);
    void Fail(int64_t now_us);
    void Interrupt(int64_t now_us);
    void RecordSample(double left_target_rpm, double right_target_rpm,
                      double left_measured_rpm, double right_measured_rpm,
                      int left_pwm_percent, int right_pwm_percent);
    TimeDriveMetrics GetMetrics() const;
    TimeDriveSnapshot Snapshot(int64_t now_us) const;

    TimeDriveState state() const { return state_; }
    TimeDriveOwner owner() const { return owner_; }
    Motion direction() const { return direction_; }
    WheelCommand wheels() const { return wheels_; }
    int speed_percent() const { return speed_percent_; }
    int duration_seconds() const { return duration_seconds_; }
    int angle_degrees() const { return angle_degrees_; }
    int ElapsedMs(int64_t now_us) const;
    int RemainingMs(int64_t now_us) const;

    static const char* StateName(TimeDriveState state);
    static const char* OwnerName(TimeDriveOwner owner);

private:
    TimeDriveState state_ = TimeDriveState::kIdle;
    TimeDriveOwner owner_ = TimeDriveOwner::kWeb;
    Motion direction_ = Motion::kStop;
    WheelCommand wheels_;
    int speed_percent_ = 0;
    int duration_seconds_ = 0;
    int angle_degrees_ = 0;
    int64_t started_us_ = 0;
    int64_t deadline_us_ = 0;
    int64_t ended_us_ = 0;
    TimeDriveMetrics totals_;
    double left_rpm_squares_ = 0;
    double right_rpm_squares_ = 0;
};

}  // namespace ruby_robot
