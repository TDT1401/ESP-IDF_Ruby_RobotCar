#pragma once

#include <driver/gpio.h>

struct UltrasonicSensorConfig {
    gpio_num_t trigger_pin;
    gpio_num_t echo_pin;
};

using Hcsr04ReadingCallback = void (*)(int distance_cm, bool valid);

// Starts one board-lifetime measurement task for a fixed forward-facing sensor.
// Samples no faster than every 60 ms.
// The callback runs on the sensor task and must be thread-safe and non-blocking.
void InitializeHcsr04Sensor(const UltrasonicSensorConfig& config, Hcsr04ReadingCallback callback);
