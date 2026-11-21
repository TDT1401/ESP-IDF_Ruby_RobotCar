#pragma once

#include <cmath>
#include <cstdint>
#include <numbers>

namespace ruby_robot {

struct OdometrySample {
    double left_m = 0;
    double right_m = 0;
    double heading_degrees = 0;
    uint64_t unknown_left_pulses = 0;
    uint64_t unknown_right_pulses = 0;
    int64_t at_us = 0;
    bool valid = false;
};

struct OdometrySnapshot {
    bool initialized = false;
    bool valid = false;
    bool wheel_heading_valid = false;
    double x_m = 0;
    double y_m = 0;
    double theta_rad = 0;  // Unwrapped; positive turns left, +X is the initial heading.
    double left_m = 0;
    double right_m = 0;
    double centre_m = 0;  // Signed wheel-centre distance, not distance from the origin.
    double wheel_theta_rad = 0;
    double velocity_mps = 0;
    double dt_s = 0;
    int64_t sampled_at_us = 0;
    const char* fault = "not_initialized";
};

// Pure dead reckoning. Wheel distances already include CPR/diameter calibration.
// Direction is inferred by the single-channel encoders, so unknown pulses or a
// feedback gap invalidate the pose until an explicit stationary reset.
class Odometry {
public:
    static constexpr int64_t kMaximumGapUs = 100000;

    bool Reset(const OdometrySample& sample, double track_width_m) {
        if (!Valid(sample) || !std::isfinite(track_width_m) || track_width_m < 0)
            return false;
        origin_ = previous_ = sample;
        track_width_m_ = track_width_m;
        pose_ = {};
        pose_.initialized = pose_.valid = true;
        pose_.wheel_heading_valid = track_width_m > 0;
        pose_.sampled_at_us = sample.at_us;
        pose_.fault = "none";
        return true;
    }

    bool Update(const OdometrySample& sample) {
        if (!pose_.valid)
            return false;
        if (!Valid(sample))
            return Invalidate("sensor_unavailable");
        if (sample.unknown_left_pulses != previous_.unknown_left_pulses ||
            sample.unknown_right_pulses != previous_.unknown_right_pulses)
            return Invalidate("unknown_direction");
        const int64_t elapsed_us = sample.at_us - previous_.at_us;
        if (elapsed_us < 0 || elapsed_us > kMaximumGapUs)
            return Invalidate("sample_gap");
        if (elapsed_us == 0)
            return true;
        const double left_delta = sample.left_m - previous_.left_m;
        const double right_delta = sample.right_m - previous_.right_m;
        const double ds = (left_delta + right_delta) * 0.5;
        const double theta =
            (sample.heading_degrees - origin_.heading_degrees) * std::numbers::pi / 180.0;
        const double midpoint = (pose_.theta_rad + theta) * 0.5;
        pose_.x_m += ds * std::cos(midpoint);
        pose_.y_m += ds * std::sin(midpoint);
        pose_.theta_rad = theta;
        pose_.left_m = sample.left_m - origin_.left_m;
        pose_.right_m = sample.right_m - origin_.right_m;
        pose_.centre_m = (pose_.left_m + pose_.right_m) * 0.5;
        if (pose_.wheel_heading_valid)
            pose_.wheel_theta_rad += (right_delta - left_delta) / track_width_m_;
        pose_.dt_s = elapsed_us / 1000000.0;
        pose_.velocity_mps = ds / pose_.dt_s;
        pose_.sampled_at_us = sample.at_us;
        previous_ = sample;
        return true;
    }

    const OdometrySnapshot& Snapshot() const { return pose_; }

private:
    OdometrySample origin_;
    OdometrySample previous_;
    OdometrySnapshot pose_;
    double track_width_m_ = 0;

    static bool Valid(const OdometrySample& sample) {
        return sample.valid && std::isfinite(sample.left_m) && std::isfinite(sample.right_m) &&
               std::isfinite(sample.heading_degrees);
    }
    bool Invalidate(const char* fault) {
        pose_.valid = pose_.wheel_heading_valid = false;
        pose_.velocity_mps = 0;
        pose_.fault = fault;
        return false;
    }
};

}  // namespace ruby_robot
