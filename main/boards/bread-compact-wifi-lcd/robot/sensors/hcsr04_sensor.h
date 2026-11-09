#pragma once

using Hcsr04ReadingCallback = void (*)(int distance_cm, bool valid);

// Starts one board-lifetime measurement task and holds the servo at center with hardware PWM.
// Samples no faster than every 60 ms.
// The callback runs on the sensor task and must be thread-safe and non-blocking.
void InitializeHcsr04Sensor(Hcsr04ReadingCallback callback);

