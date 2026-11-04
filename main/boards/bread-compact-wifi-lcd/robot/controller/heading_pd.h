#pragma once

#include <algorithm>

namespace ruby_robot {

struct HeadingPdTargets {
    double left_rpm;
    double right_rpm;
    double error_degrees;
    double correction_rpm;
};

// Positive heading and yaw rate mean a left turn. A positive correction makes
// the right wheel faster than the left wheel, including while driving backward.
class HeadingPd {
public:
    static HeadingPdTargets Update(double base_signed_rpm, double target_degrees,
                                   double measured_degrees, double yaw_rate_dps, double kp,
                                   double kd, double maximum_correction_rpm) {
        const double error = target_degrees - measured_degrees;
        const double correction = std::clamp(kp * error - kd * yaw_rate_dps,
                                             -maximum_correction_rpm, maximum_correction_rpm);
        return {base_signed_rpm - correction, base_signed_rpm + correction, error, correction};
    }
};

}  // namespace ruby_robot
