#pragma once

#include <sdkconfig.h>

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
// Phase 1: closed-loop wheel speed control.
// Provisional Phase 1 reference: motor specification is about 175 RPM at 6 V
// under load (90 RPM at 3 V, 200 RPM at 6 V unloaded). Assume 6 V motor supply
// and use the loaded figure for both wheels at full PWM until FC-03 readings
// allow separate per-wheel calibration. The L298N voltage drop and actual load
// can make the achievable wheel RPM lower than this estimate.
#define SPEED_PI_REFERENCE_PWM_PERCENT 100
#define SPEED_PI_LEFT_REFERENCE_RPM 200.0
#define SPEED_PI_RIGHT_REFERENCE_RPM 200.0
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
#endif
