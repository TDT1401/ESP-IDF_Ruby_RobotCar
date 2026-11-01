#pragma once

#include <algorithm>
#include <cmath>

namespace ruby_robot {

// One wheel, in PWM percent and wheel RPM. The caller owns timing and direction.
class SpeedPi {
public:
    void Reset() { integral_ = 0; }

    int Update(double target_rpm, double measured_rpm, double feedforward_percent, double dt_s,
               double kp, double ki) {
        const double error = target_rpm - measured_rpm;
        const double candidate = std::clamp(integral_ + ki * error * dt_s, -100.0, 100.0);
        const double unconstrained = feedforward_percent + kp * error + candidate;
        if (!((unconstrained > 100.0 && error > 0) || (unconstrained < 0 && error < 0)))
            integral_ = candidate;
        const double output = std::clamp(feedforward_percent + kp * error + integral_, 0.0, 100.0);
        return static_cast<int>(std::lround(output));
    }

private:
    double integral_ = 0;
};

}  // namespace ruby_robot
