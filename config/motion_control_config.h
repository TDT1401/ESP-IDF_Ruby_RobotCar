#pragma once

#include <sdkconfig.h>

// Phase 1: closed-loop wheel speed control.
#define SPEED_PI_REFERENCE_PWM_PERCENT 100
#define SPEED_PI_LEFT_REFERENCE_RPM 200.0
#define SPEED_PI_RIGHT_REFERENCE_RPM 200.0
// Initial gains in PWM-percent/RPM and PWM-percent/(RPM*s); tune on the car.
#define SPEED_PI_LEFT_KP 0.25
#define SPEED_PI_LEFT_KI 0.50
#define SPEED_PI_RIGHT_KP 0.25
#define SPEED_PI_RIGHT_KI 0.50

// Phase 2: heading control and angle turns.
#define HEADING_PD_KP_RPM_PER_DEG 0.8
#define HEADING_PD_KD_RPM_PER_DPS 0.15
#define HEADING_PD_MAX_CORRECTION_RPM 35.0

// Shared automatic turn profile. RPM follows angle error with gyro damping;
// the maximum is a ceiling, not a fixed speed. Tune on the floor.
#define HEADING_TURN_MAX_RPM 200.0
#define HEADING_TURN_KP_RPM_PER_DEG 3.0
#define HEADING_TURN_ACCELERATION_RPM_PER_S 100.0
#define HEADING_TURN_DECELERATION_RPM_PER_S 200.0
#define HEADING_TURN_MIN_RPM 20.0
// Stop within the angle window and wait for a low yaw rate before completing.
#define HEADING_TURN_TOLERANCE_DEGREES 2.0
#define HEADING_TURN_SETTLE_RATE_DPS 8.0
#define HEADING_TURN_SETTLE_RPM 3.0
// Provisional friction feedforward for skid turns, in PWM percent. Tune each side
// on the floor. PI may reduce output below this; a zero RPM target always stops.
#define HEADING_TURN_LEFT_FRICTION_PWM_PERCENT 60.0
#define HEADING_TURN_RIGHT_FRICTION_PWM_PERCENT 60.0
// Set to 1 to log heading feedback, wheel RPM targets/measurements and PWM at 5 Hz.
#define HEADING_PD_DEBUG 0

// Phase 3: distance and pose control.
#define ODOMETRY_TRACK_WIDTH_MM 0.0
// Enable only after a measured B_eff and independent EKF/odometry trials show
// lower, stable pose error. Zero keeps navigation on the established odometry.
#define NAVIGATION_USE_EKF 0
// CSV pose/distance telemetry at 5 Hz, outside the motor timer task.
#define ODOMETRY_DEBUG 0

// Provisional distance profile; tune against independent floor measurements.
// Dùng cho các lệnh di chuyển khoảng cách, không dùng cho các lệnh quay góc.
// Shared automatic profile limits for Web, MCP and direct distance requests.
// The maximum is a ceiling; short moves may never reach it.
#define DISTANCE_DRIVE_MAX_SPEED_PERCENT 100
#define DISTANCE_DRIVE_ACCELERATION_MPS2 0.30
#define DISTANCE_DRIVE_DECELERATION_MPS2 0.30
#define DISTANCE_DRIVE_MIN_SPEED_PERCENT 15
// Initial PWM estimate for slow distance moves. Tune each wheel on the floor;
// Speed PI may still reduce PWM when the measured RPM exceeds its target.
#define DISTANCE_DRIVE_LEFT_FRICTION_PWM_PERCENT 10.0
#define DISTANCE_DRIVE_RIGHT_FRICTION_PWM_PERCENT 10.0
#define DISTANCE_DRIVE_TOLERANCE_MM 20.0
#define DISTANCE_DRIVE_SETTLE_RPM 3.0
#define DISTANCE_DRIVE_SETTLE_MS 300
