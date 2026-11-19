#pragma once

#include <driver/gpio.h>
#include <driver/ledc.h>

struct UltrasonicSensorConfig {
    gpio_num_t trigger_pin;
    gpio_num_t echo_pin;
    gpio_num_t servo_pin;
    int servo_center_pulse_us;
    ledc_mode_t servo_mode;
    ledc_timer_t servo_timer;
    ledc_channel_t servo_channel;
};

using Hcsr04ReadingCallback = void (*)(int distance_cm, bool valid);

// Starts one board-lifetime measurement task and holds the servo at center with hardware PWM.
// Samples no faster than every 60 ms.
// The callback runs on the sensor task and must be thread-safe and non-blocking.
void InitializeHcsr04Sensor(const UltrasonicSensorConfig& config, Hcsr04ReadingCallback callback);
