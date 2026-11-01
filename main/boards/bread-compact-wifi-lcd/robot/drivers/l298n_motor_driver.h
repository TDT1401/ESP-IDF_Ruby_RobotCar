#pragma once

#include <esp_err.h>

namespace ruby_robot {

enum class Wheel { kLeft, kRight };

// Owns only the L298N GPIO/LEDC outputs. Motion timing, ownership and safety
// remain in the controller above this hardware layer.
class L298nMotorDriver {
public:
    L298nMotorDriver();

    // Disable both bridges before changing direction pins. Attempt all stop
    // operations even when an earlier one fails.
    esp_err_t Stop();
    esp_err_t SetPwm(Wheel wheel, int percent);
    esp_err_t SetDirection(Wheel wheel, int sign, bool inverted);
};

}  // namespace ruby_robot
