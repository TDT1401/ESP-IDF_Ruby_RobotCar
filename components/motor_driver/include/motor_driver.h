#pragma once

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_err.h>

#include <array>
#include <cstdint>

namespace ruby_robot {

// Left/right are the front wheels carrying the encoders.
enum class Wheel { kLeft, kRight, kRearLeft, kRearRight };

struct MotorConfig {
    gpio_num_t enable;
    gpio_num_t in1;
    gpio_num_t in2;
    ledc_channel_t channel;
    bool inverted;
};

struct MotorDriverConfig {
    // Order matches Wheel: front left/right, rear left/right.
    std::array<MotorConfig, 4> motors;
    ledc_mode_t mode;
    ledc_timer_t timer;
    ledc_timer_bit_t duty_resolution;
    uint32_t freq_hz;
};

// Owns only the L298N GPIO/LEDC outputs. Motion timing, ownership and safety
// remain in the controller above this hardware layer.
class MotorDriver {
public:
    explicit MotorDriver(const MotorDriverConfig& config);

    // Disable all bridges before changing direction pins. Attempt all stop
    // operations even when an earlier one fails.
    esp_err_t Stop();
    esp_err_t SetPwm(Wheel wheel, int percent);
    // Accept logical wheel signs; apply the board's wiring inversion here.
    esp_err_t SetDirection(Wheel wheel, int sign);

private:
    MotorDriverConfig config_;

    const MotorConfig* Pins(Wheel wheel) const;
};

}  // namespace ruby_robot
