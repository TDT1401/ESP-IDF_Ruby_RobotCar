#pragma once

#include <esp_err.h>

namespace ruby_robot {

// Left/right are the front wheels carrying the encoders.
enum class Wheel { kLeft, kRight, kRearLeft, kRearRight };

// Owns only the L298N GPIO/LEDC outputs. Motion timing, ownership and safety
// remain in the controller above this hardware layer.
class L298nMotorDriver {
public:
    L298nMotorDriver();

    // Disable all bridges before changing direction pins. Attempt all stop
    // operations even when an earlier one fails.
    esp_err_t Stop();
    esp_err_t SetPwm(Wheel wheel, int percent);
    // Accept logical wheel signs; apply the board's wiring inversion here.
    esp_err_t SetDirection(Wheel wheel, int sign);
};

}  // namespace ruby_robot
