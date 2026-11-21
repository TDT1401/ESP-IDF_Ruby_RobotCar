# Cấu trúc motor_controller

```text
motor_controller/
├── include/
│   ├── motor_controller.h       # API công khai, request và snapshot
│   ├── control/motion_policy.h
│   ├── distance/distance_drive.h
│   ├── manual/web_control_session.h
│   └── odometry/
│       ├── odometry.h
│       └── pose_ekf.h
├── manual/
│   └── manual_control.cc        # Điều khiển thủ công, session, heartbeat
├── distance/
│   └── distance_drive.cc        # Quãng đường, tiến độ, profile tốc độ
├── control/
│   ├── motion_executor.cc       # Vòng thực thi chung và quay góc
│   ├── speed_control.cc         # Speed PI, giữ hướng, profile quay
│   ├── speed_pi.h
│   ├── heading_pd.h
│   └── motor_output.cc
├── odometry/
│   └── odometry_control.cc      # Lấy mẫu, cập nhật/reset odometry và EKF
├── safety/
│   └── sensor_behavior.cc       # Khoảng cách ultrasonic và âm cảnh báo
├── motor_controller.cc         # Khởi tạo và điều phối
├── controller_internal.h
└── CMakeLists.txt
```

- Manual: đọc `manual/manual_control.cc`, sau đó `control/motion_executor.cc`.
- Quãng đường: đọc `distance/distance_drive.cc`, sau đó vòng thực thi và
  `control/speed_control.cc`.
- Quay góc: đọc nhánh `AngleTurnRequest` trong `motor_controller.cc`, sau đó
  `UpdateTurnTargetsLocked()` trong `control/speed_control.cc`.
- Odometry/EKF và quỹ đạo XY được giữ độc lập với các mục tiêu chuyển động.
