# RobotCar Board Integration

## Cấu hình cố định

- Một board ESP32-S3-N16R8, một firmware build.
- Factory duy nhất: `DECLARE_BOARD(RobotCarBoard)` tại `main/robot_car_board.cc`.
- `config/build_config.json`: target `esp32s3`; giữ `type` và `builds[0].name` là `bread-compact-wifi-lcd` cho Xiaozhi/OTA.
- Giữ pin, Kconfig keys, NVS keys và partition `partitions/v2/16m.csv`.

## Nơi chỉnh cấu hình

| File | Nội dung |
| --- | --- |
| `config/board_config.h` | Tổng hợp cấu hình board |
| `config/robot_hardware_config.h` | Pin và hiệu chuẩn cảm biến |
| `config/motion_control_config.h` | PI/PD, motion và odometry/EKF |
| `config/audio_config.h` | I2S và audio |
| `config/telegram_config.local.h` | Credentials riêng, được Git bỏ qua |

Board khởi tạo robot tại `main/robot/robot_board.cc`, truyền struct/callback vào component. `config/` là private include path của `main`.

## Phần cứng và dịch vụ

- Hai L298N, bốn kênh motor; điều khiển logic trái/phải. Bánh sau theo PWM bánh trước cùng bên, encoder chỉ ở hai bánh trước.
- HC-SR04 đo phía trước, servo giữ giữa. LEDC: motor timer 1, channel 1/2/4/5; servo timer/channel 2/3.
- Web tích hợp sẵn ở port `8080`: `/api/claim`, `/api/command`, `/api/action`, `/api/status`.
- Telegram tích hợp sẵn, chỉ gửi; board cung cấp credentials và callback readiness.

## Build và nạp

Ưu tiên ESP-IDF v6.1, tối thiểu v6.0.1; không hỗ trợ IDF 5.x. Khởi tạo môi trường IDF, sao chép `config/telegram_config.example.h` thành `config/telegram_config.local.h` và điền credentials. TLS certificate bundle phải bật; không đưa credentials vào Git.

Chạy thủ công tại thư mục gốc:

```sh
python scripts/build.py
idf.py -p <SERIAL_PORT> flash monitor
```

Build dùng `build/` và có thể cập nhật `sdkconfig`. Có thể thêm `--language vi-VN --wake-word wn9_hiesp`; không truyền board/variant.

Xem [wiring](robot/BOARD_GUIDE.vi.md), [kiến trúc](ARCHITECTURE.md) và [tiến độ](robot/ROBOT_4WD_REQUIREMENTS.md). Sau thay đổi liên quan, cần xác nhận chuyển động và safety trên xe.
