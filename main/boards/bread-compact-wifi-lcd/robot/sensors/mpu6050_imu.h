#pragma once

#include <esp_err.h>

#include <cstdint>

struct Mpu6050Status {
    bool ready = false;
    bool heading_valid = false;  // False after a sample gap too long to integrate safely.
    // Relative to the heading when startup calibration completes; left is positive.
    double heading_degrees = 0;
    double gyro_z_dps = 0;
    double bias_z_dps = 0;
    int64_t sampled_at_us = 0;
    esp_err_t error = ESP_OK;
};

// Starts one board-lifetime I2C sampling task. Calibration needs a stationary car
// for roughly two seconds after boot; sampling never runs in the motor timer.
void InitializeMpu6050();
void SetMpu6050MotorActive(bool active);
Mpu6050Status GetMpu6050Status();
