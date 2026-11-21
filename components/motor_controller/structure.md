
```
motor_controller/
├── include/
│   └── motor_controller.h       # API công khai
│
├── manual/
│   └── manual_control.cc        # Điều khiển thủ công, session, heartbeat
│
├── distance/
│   ├── distance_drive.h
│   └── distance_drive.cc        # Mục tiêu quãng đường, tiến độ, tăng/giảm tốc
│
├── navigation/
│   ├── navigator.h
│   └── navigator.cc             # Đi đến tọa độ, tính hướng và khoảng cách
│
├── control/
│   ├── motion_executor.cc      # Vòng thực thi chung
│   ├── speed_control.cc
│   ├── speed_pi.h
│   ├── heading_pd.h
│   └── motor_output.cc
│
├── odometry/
│   └── ...                     # Ước lượng vị trí
├── safety/
│   └── ...
├── motor_controller.cc         # Khởi tạo và điều phối
├── controller_internal.h
└── CMakeLists.txt
```