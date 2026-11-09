#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Set to 1 to log microphone levels every 2 seconds, then rebuild and flash.
// Startup configuration and I2S read errors are logged even when this is 0.
#define MIC_DEBUG 0

#define AUDIO_I2S_MIC_GPIO_WS GPIO_NUM_1
#define AUDIO_I2S_MIC_GPIO_SCK GPIO_NUM_2
#define AUDIO_I2S_MIC_GPIO_DIN GPIO_NUM_17
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
// Reserve GPIO48 for the right encoder; do not drive the onboard LED on this pin.
#define BUILTIN_LED_GPIO GPIO_NUM_NC
#else
#define BUILTIN_LED_GPIO GPIO_NUM_48
#endif
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define TOUCH_BUTTON_GPIO GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_42
#define DISPLAY_MOSI_PIN GPIO_NUM_47
#define DISPLAY_CLK_PIN GPIO_NUM_21
#define DISPLAY_DC_PIN GPIO_NUM_40
#define DISPLAY_RST_PIN GPIO_NUM_45
#define DISPLAY_CS_PIN GPIO_NUM_41

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR true
#define DISPLAY_RGB_ORDER LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0
// Set the ST7789 RAM window before LVGL starts.
#define DISPLAY_PANEL_GAP_X 0
#define DISPLAY_PANEL_GAP_Y (DISPLAY_MIRROR_Y ? (320 - DISPLAY_HEIGHT) : 0)
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#define DISPLAY_SPI_CLOCK_HZ (40 * 1000 * 1000)

// No lamp is connected; this pin remains unavailable to the car.
#define LAMP_GPIO GPIO_NUM_NC

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
// GPIO4/5 are free with the simplex I2S configuration above.
#define MPU6050_SDA_PIN GPIO_NUM_4
#define MPU6050_SCL_PIN GPIO_NUM_5
#define MPU6050_I2C_ADDRESS 0x68
// Set to 1 to log heading, gyro Z, bias and MPU6050 diagnostics to Serial Monitor.
#define MPU6050_DEBUG 0
// Flat module, component side up: a physical left turn must give positive gyro Z.
// Set to -1.0 only if the module's Z axis points down.
#define MPU6050_GYRO_Z_SIGN 1.0
#define MOTOR_LEFT_EN_PIN GPIO_NUM_8
#define MOTOR_LEFT_IN1_PIN GPIO_NUM_9
#define MOTOR_LEFT_IN2_PIN GPIO_NUM_10
#define MOTOR_RIGHT_IN1_PIN GPIO_NUM_11
#define MOTOR_RIGHT_IN2_PIN GPIO_NUM_12
#define MOTOR_RIGHT_EN_PIN GPIO_NUM_13

// Phase 1: closed-loop wheel speed control.
// Provisional Phase 1 reference: motor specification is about 175 RPM at 6 V
// under load (90 RPM at 3 V, 200 RPM at 6 V unloaded). Assume 6 V motor supply
// and use the loaded figure for both wheels at full PWM until FC-03 readings
// allow separate per-wheel calibration. The L298N voltage drop and actual load
// can make the achievable wheel RPM lower than this estimate.
#define SPEED_PI_REFERENCE_PWM_PERCENT 100
#define SPEED_PI_LEFT_REFERENCE_RPM 175.0
#define SPEED_PI_RIGHT_REFERENCE_RPM 175.0
// Initial gains in PWM-percent/RPM and PWM-percent/(RPM*s); tune on the car.
#define SPEED_PI_LEFT_KP 0.25
#define SPEED_PI_LEFT_KI 0.50
#define SPEED_PI_RIGHT_KP 0.25
#define SPEED_PI_RIGHT_KI 0.50

// Phase 2: heading control and angle turns.
// Initial straight-heading PD gains in RPM/degree and RPM/(degree/s); tune on the car.
// The correction is also limited to half the base wheel RPM to preserve direction.
#define HEADING_PD_KP_RPM_PER_DEG 0.8
#define HEADING_PD_KD_RPM_PER_DPS 0.15
#define HEADING_PD_MAX_CORRECTION_RPM 35.0

// Angle turns use the requested speed as an RPM ceiling.
// Use this default when an angle turn does not specify a speed.
#define HEADING_TURN_AUTO_SPEED_PERCENT 70
// Turn P gain = requested RPM / slowdown angle; gyro damping uses HEADING_PD_KD above.
#define HEADING_TURN_SLOWDOWN_DEGREES 45.0
#define HEADING_TURN_MIN_RPM 20.0
// Stop within the angle window and wait for a low yaw rate before completing.
#define HEADING_TURN_TOLERANCE_DEGREES 2.0
#define HEADING_TURN_SETTLE_RATE_DPS 8.0
// Provisional friction feedforward for skid turns, in PWM percent. Tune each side
// on the floor. PI may reduce output below this; a zero RPM target always stops.
#define HEADING_TURN_LEFT_FRICTION_PWM_PERCENT 60.0
#define HEADING_TURN_RIGHT_FRICTION_PWM_PERCENT 60.0
// Set to 1 to log heading feedback, wheel RPM targets/measurements and PWM at 5 Hz.
#define HEADING_PD_DEBUG 0

// Phase 3: distance and pose control.
// Effective skid-steer track width, not necessarily the physical wheel spacing.
// Calibrate in mm: 1000 * (right_m - left_m) / independent turn angle (radians).
// Zero keeps encoder-only heading unavailable; gyro-based pose/distance still work.
#define ODOMETRY_TRACK_WIDTH_MM 0.0
// Enable only after a measured B_eff and independent EKF/odometry trials show
// lower, stable pose error. Zero keeps navigation on the established odometry.
#define NAVIGATION_USE_EKF 0
// CSV pose/distance telemetry at 5 Hz, outside the motor timer task.
#define ODOMETRY_DEBUG 0

// Provisional distance profile; tune against independent floor measurements.
// Dùng cho các lệnh di chuyển khoảng cách, không dùng cho các lệnh quay góc.
#define DISTANCE_DRIVE_MAX_SPEED_PERCENT 70
#define DISTANCE_DRIVE_ACCELERATION_MPS2 0.30
#define DISTANCE_DRIVE_DECELERATION_MPS2 0.30
#define DISTANCE_DRIVE_MIN_SPEED_PERCENT 18
// Initial PWM estimate for slow distance moves. Tune each wheel on the floor;
// Speed PI may still reduce PWM when the measured RPM exceeds its target.
#define DISTANCE_DRIVE_LEFT_FRICTION_PWM_PERCENT 62.0
#define DISTANCE_DRIVE_RIGHT_FRICTION_PWM_PERCENT 62.0
#define DISTANCE_DRIVE_TOLERANCE_MM 20.0
#define DISTANCE_DRIVE_SETTLE_RPM 3.0
#define DISTANCE_DRIVE_SETTLE_MS 300

// HC-SR04 uses 5 V power. ECHO must be level-shifted to 3.3 V before this GPIO.
#define HCSR04_TRIGGER_PIN GPIO_NUM_14
#define HCSR04_ECHO_PIN GPIO_NUM_39

// Keep the HC-SR04 aimed forward until servo scanning is added in Phase 7.
#define HCSR04_SERVO_SIGNAL_PIN GPIO_NUM_6
#define HCSR04_SERVO_CENTER_PULSE_US 1500
#endif

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
// Phase 3: FC-03 encoder inputs and wheel-odometry calibration.
// Keep GPIO19/20 available for native USB. GPIO48's LED driver is disabled above.
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

#endif

#endif  // _BOARD_CONFIG_H_
