#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "odometry.h"

namespace ruby_robot {

struct EkfSnapshot {
    bool initialized = false;
    bool valid = false;
    double x_m = 0;
    double y_m = 0;
    double theta_rad = 0;
    double variance_x_m2 = 0;
    double variance_y_m2 = 0;
    double variance_theta_rad2 = 0;
    int64_t sampled_at_us = 0;
    const char* fault = "not_initialized";
};

// Three-state local EKF: wheel increments predict translation and yaw; the
// calibrated, unwrapped gyro heading corrects yaw. Neither sensor observes
// absolute position, so covariance grows during travel and cannot cure slip.
class PoseEkf {
public:
    bool Reset(const OdometrySample& sample, double track_width_m) {
        if (!std::isfinite(track_width_m) || track_width_m <= 0) {
            pose_.fault = "track_width_unconfigured";
            return false;
        }
        if (!Valid(sample))
            return false;
        origin_ = previous_ = sample;
        track_width_m_ = track_width_m;
        pose_ = {};
        pose_.initialized = pose_.valid = true;
        pose_.sampled_at_us = sample.at_us;
        pose_.fault = "none";
        for (auto& row : covariance_)
            for (double& value : row)
                value = 0;
        covariance_[0][0] = covariance_[1][1] = 0.0001;  // 1 cm origin uncertainty.
        covariance_[2][2] = 0.0003;
        PublishCovariance();
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
        if (elapsed_us < 0 || elapsed_us > Odometry::kMaximumGapUs)
            return Invalidate("sample_gap");
        if (elapsed_us == 0)
            return true;

        const double dl = sample.left_m - previous_.left_m;
        const double dr = sample.right_m - previous_.right_m;
        const double ds = (dl + dr) * 0.5;
        const double dtheta = (dr - dl) / track_width_m_;
        const double mid = pose_.theta_rad + dtheta * 0.5;
        const double s = std::sin(mid);
        const double c = std::cos(mid);
        pose_.x_m += ds * c;
        pose_.y_m += ds * s;
        pose_.theta_rad += dtheta;

        // F = derivative of the midpoint motion model with respect to pose.
        const double f[3][3] = {{1, 0, -ds * s}, {0, 1, ds * c}, {0, 0, 1}};
        double propagated[3][3] = {};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    for (int l = 0; l < 3; ++l)
                        propagated[i][j] += f[i][k] * covariance_[k][l] * f[j][l];
        // Conservative wheel and skid noise, including a nonzero noise floor.
        const double travel = std::abs(dl) + std::abs(dr);
        const double translation_variance = 0.000001 + 0.0025 * travel * travel;
        const double yaw_variance = 0.000001 + 0.01 * travel * travel /
                                                  (track_width_m_ * track_width_m_);
        propagated[0][0] += translation_variance;
        propagated[1][1] += translation_variance;
        propagated[2][2] += yaw_variance;

        const double gyro_heading =
            (sample.heading_degrees - origin_.heading_degrees) * std::numbers::pi / 180.0;
        // Gyro bias error is correlated over time. Inflate its yaw uncertainty
        // with elapsed time rather than treating every integrated heading as
        // a fresh absolute angle observation.
        const double age_s = (sample.at_us - origin_.at_us) / 1000000.0;
        const double bias_error_rad = 0.0087 * age_s;  // Provisional 0.5 deg/s.
        const double measurement_variance = 0.0003 + bias_error_rad * bias_error_rad;
        const double innovation_variance = propagated[2][2] + measurement_variance;
        const double gain[3] = {propagated[0][2] / innovation_variance,
                                propagated[1][2] / innovation_variance,
                                propagated[2][2] / innovation_variance};
        const double innovation = gyro_heading - pose_.theta_rad;
        pose_.x_m += gain[0] * innovation;
        pose_.y_m += gain[1] * innovation;
        pose_.theta_rad += gain[2] * innovation;
        // Joseph form keeps covariance symmetric and nonnegative after update.
        double updated[3][3] = {};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                updated[i][j] = propagated[i][j] - gain[i] * propagated[2][j] -
                                propagated[i][2] * gain[j] +
                                gain[i] * innovation_variance * gain[j];
                if (!std::isfinite(updated[i][j]))
                    return Invalidate("numerical_error");
            }
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                covariance_[i][j] = updated[i][j];
        if (!std::isfinite(pose_.x_m) || !std::isfinite(pose_.y_m) ||
            !std::isfinite(pose_.theta_rad))
            return Invalidate("numerical_error");
        pose_.sampled_at_us = sample.at_us;
        previous_ = sample;
        PublishCovariance();
        return true;
    }

    const EkfSnapshot& Snapshot() const { return pose_; }

private:
    static bool Valid(const OdometrySample& sample) {
        return sample.valid && std::isfinite(sample.left_m) && std::isfinite(sample.right_m) &&
               std::isfinite(sample.heading_degrees);
    }
    bool Invalidate(const char* fault) {
        pose_.valid = false;
        pose_.fault = fault;
        return false;
    }
    void PublishCovariance() {
        pose_.variance_x_m2 = std::max(0.0, covariance_[0][0]);
        pose_.variance_y_m2 = std::max(0.0, covariance_[1][1]);
        pose_.variance_theta_rad2 = std::max(0.0, covariance_[2][2]);
    }

    OdometrySample origin_;
    OdometrySample previous_;
    EkfSnapshot pose_;
    double covariance_[3][3] = {};
    double track_width_m_ = 0;
};

}  // namespace ruby_robot
