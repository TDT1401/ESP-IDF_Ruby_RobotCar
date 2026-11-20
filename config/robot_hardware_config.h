#pragma once

#include <driver/gpio.h>
#include <sdkconfig.h>

// GPIO4/5 are free with the simplex I2S configuration in audio_config.h.
#define MPU6050_SDA_PIN GPIO_NUM_4
#define MPU6050_SCL_PIN GPIO_NUM_5
#define MPU6050_I2C_ADDRESS 0x68
// Set to 1 to log heading, gyro Z, bias and MPU6050 diagnostics to Serial Monitor.
#define MPU6050_DEBUG 0
// Flat module, component side up: a physical left turn must give positive gyro Z.
// Set to -1.0 only if the module's Z axis points down.
#define MPU6050_GYRO_Z_SIGN 1.0
// Left/right denote the front wheels, where the FC-03 encoders are mounted.
#define MOTOR_LEFT_EN_PIN GPIO_NUM_13
#define MOTOR_LEFT_IN1_PIN GPIO_NUM_11
#define MOTOR_LEFT_IN2_PIN GPIO_NUM_12
// GPIO45 is a boot strap: remove the ENA jumper and add a 10 kOhm pull-down.
#define MOTOR_RIGHT_EN_PIN GPIO_NUM_45
#define MOTOR_RIGHT_IN1_PIN GPIO_NUM_21
#define MOTOR_RIGHT_IN2_PIN GPIO_NUM_40
#define MOTOR_REAR_LEFT_EN_PIN GPIO_NUM_8
#define MOTOR_REAR_LEFT_IN1_PIN GPIO_NUM_9
#define MOTOR_REAR_LEFT_IN2_PIN GPIO_NUM_10
#define MOTOR_REAR_RIGHT_EN_PIN GPIO_NUM_47
#define MOTOR_REAR_RIGHT_IN1_PIN GPIO_NUM_41
#define MOTOR_REAR_RIGHT_IN2_PIN GPIO_NUM_42

// HC-SR04 uses 5 V power. ECHO must be level-shifted to 3.3 V before this GPIO.
#define HCSR04_TRIGGER_PIN GPIO_NUM_14
#define HCSR04_ECHO_PIN GPIO_NUM_39

// Keep the HC-SR04 aimed forward until servo scanning is added in Phase 7.
#define HCSR04_SERVO_SIGNAL_PIN GPIO_NUM_6
#define HCSR04_SERVO_CENTER_PULSE_US 1500

// Phase 3: FC-03 encoder inputs and wheel-odometry calibration.
// Keep GPIO19/20 available for native USB. board_config.h disables GPIO48's LED driver.
#define FC03_LEFT_GPIO GPIO_NUM_38
#define FC03_RIGHT_GPIO GPIO_NUM_48

// Calibration: count rising edges during ONE complete WHEEL revolution.
// Include the gearbox ratio if the encoder disc is on the motor shaft.
// Zero means unknown: raw pulse counts work, but metres/RPM remain unavailable.
#define FC03_LEFT_PULSES_PER_REV 20.01
#define FC03_RIGHT_PULSES_PER_REV 20.01
#define FC03_LEFT_WHEEL_DIAMETER_MM 65.0
#define FC03_RIGHT_WHEEL_DIAMETER_MM 65.0

// PCNT rejects pulses shorter than this (ESP32-S3: keep <= 12000 ns).
#define FC03_GLITCH_FILTER_NS 10000
// Continue inferring the previous direction briefly after PWM stops (coasting).
#define FC03_COAST_MS 500
// Log both counters every 2 seconds in monitor; set to 0 after calibration.
#define FC03_DEBUG 0
