# Bread Compact Wi-Fi LCD

[English](README.md) | **Tiếng Việt**

Bo mạch ESP32-S3-N16R8, microphone I2S, MAX98357A và màn hình TFT SPI.

## Chức năng

- Trợ lý giọng nói, lái thủ công qua web/HUD, trạng thái và lệnh dừng qua Telegram.
- Tiến, lùi và xoay bằng L298N; hỗ trợ PI tốc độ riêng cho từng bánh.
- HC-SR04 đo khoảng cách; FC-03 báo xung/RPM; MPU6050 báo tốc độ quay Z.

Tạm ngừng lệnh đi theo quãng đường, quay theo góc, giữ hướng PD,
follow và tự hành né vật cản trong lúc thiết kế lại bộ điều khiển. Mất lệnh web
trong 600 ms sẽ dừng motor.

## Cấu trúc mã nguồn

Mã robot nằm trong `robot/`. `l298n_motor_driver.cc` xuất PWM,
`motion_executor.cc` quản lý thời điểm bắt đầu/dừng lệnh thủ công,
`web_control.cc` quản lý phiên web. Các sensor chỉ cung cấp số đo, không
điều chỉnh PWM. Chân GPIO nằm trong `config.h`.

Chạy kiểm thử trang web từ thư mục gốc:

```sh
node --test main/boards/bread-compact-wifi-lcd/robot/tests/web_page_test.cjs
```
## Component

| Số lượng | Linh kiện                 | Ghi chú        |
| -------- | ------------------------- | -------------- |
| 1        | Khung xe (chassis)        |                |
| 4        | Động cơ DC                |                |
| 4        | Bánh xe                   |                |
| 1        | Driver motor L298N        |                |
| 1        | Loa                       |                |
| 1        | Mạch khuếch đại MAX98357A | Mạch loa       |
| 1        | Microphone I2S            |                |
| 1        | Bo mạch ESP32-S3          | ESP32-S3-N16R8 |
| 1        | Màn hình TFT SPI          |                |
| 1        | IMU MPU6050              |                |
| 2        | Pin 18650                 | 3,7 V mỗi cell |
| 1        | Công tắc nguồn            |                |

## Nối dây

Nối GND chung giữa ESP32-S3, các module và nguồn motor. Dùng đúng mức điện áp ghi trong từng bảng.

### Microphone I2S

| Chân microphone | Chân ESP32-S3 |
| --------------- | ------------- |
| VDD             | 3V3           |
| GND             | GND           |
| WS              | GPIO1         |
| SCK             | GPIO2         |
| SD              | GPIO17        |
| L/R             | GND           |

### Mạch khuếch đại MAX98357A

| Chân MAX98357A | Kết nối     |
| -------------- | ----------- |
| VIN            | 5V          |
| GND            | GND         |
| DIN            | GPIO7       |
| BCLK           | GPIO15      |
| LRC            | GPIO16      |
| `+` / `-`      | Hai cực loa |
| SD             | Để hở       |
| GAIN           | Để hở       |

### Màn hình TFT SPI

| Chân TFT | Chân ESP32-S3 |
| -------- | ------------- |
| GND      | GND           |
| VCC      | 3V3           |
| SCL      | GPIO21        |
| SDA      | GPIO47        |
| RES      | GPIO45        |
| DC       | GPIO40        |
| CS       | GPIO41        |
| BLK      | GPIO42        |

### Driver motor L298N

| Kênh motor | Chân L298N | Kết nối            | Chức năng        |
| ---------- | ---------- | ------------------ | ---------------- |
| Trái       | ENA        | GPIO8              | PWM motor trái   |
| Trái       | IN1        | GPIO9              | Chiều motor trái |
| Trái       | IN2        | GPIO10             | Chiều motor trái |
| Phải       | IN3        | GPIO11             | Chiều motor phải |
| Phải       | IN4        | GPIO12             | Chiều motor phải |
| Phải       | ENB        | GPIO13             | PWM motor phải   |
| Nguồn      | GND        | GND chung          | Mass nguồn motor |
| Trái       | OUT1, OUT2 | Hai dây motor trái | Ngõ ra motor     |
| Phải       | OUT3, OUT4 | Hai dây motor phải | Ngõ ra motor     |

- Tháo jumper `ENA`/`ENB`; không đưa 5 V vào GPIO.
- Cấp nguồn motor riêng qua L298N và nối chung GND.
- Khuyến nghị kéo `ENA` và `ENB` xuống GND bằng điện trở 10 kΩ.

### Cảm biến siêu âm HC-SR04

Bản build duy nhất `bread-compact-wifi-lcd` bật HC-SR04 và L298N.

| Chân HC-SR04 | Chân ESP32-S3 | Ghi chú                                      |
| ------------ | ------------- | -------------------------------------------- |
| VCC          | 5V            | Không cấp từ 3V3                             |
| GND          | GND           | Phải nối chung GND                           |
| TRIG         | GPIO14        | Tín hiệu ra từ ESP32-S3                      |
| ECHO         | GPIO39        | Bắt buộc hạ mức 5 V xuống 3,3 V trước GPIO39 |

Có thể dùng cầu chia áp cho `ECHO`, ví dụ 1 kΩ từ ECHO đến GPIO39 và 2 kΩ từ GPIO39 xuống GND.
Không nối trực tiếp `ECHO` 5 V vào ESP32-S3.

### Hai cảm biến bánh xe FC-03

| Chân FC-03 | Bánh trái | Bánh phải |
| ---------- | --------- | --------- |
| VCC        | 3V3       | 3V3       |
| GND        | GND chung | GND chung |
| D0 / DO    | GPIO38    | GPIO48    |
| A0 / AO    | Để trống  | Để trống  |

D0 phải ở mức 3,3 V. Khi bật FC-03, LED tích hợp trên GPIO48 không hoạt động.
Hiệu chuẩn trong [config.h](../config.h): `FC03_LEFT_PULSES_PER_REV` và
`FC03_RIGHT_PULSES_PER_REV` là số xung/vòng bánh; `FC03_LEFT_WHEEL_DIAMETER_MM` và
`FC03_RIGHT_WHEEL_DIAMETER_MM` là đường kính bánh (mm). Hiện cả hai bánh đặt
**20,01 xung/vòng** và đường kính **65 mm** cho mỗi bánh.

### Phase 1: PI tốc độ hai bánh

Nút hướng và joystick dùng mục tiêu RPM riêng cho bánh trái/phải. Giá trị ban đầu
trong [config.h](../config.h) là **175 RPM mỗi bánh tại PWM 100%**, lấy từ thông
số motor 6 V có tải (khoảng 90 RPM ở 3 V và 200 RPM ở 6 V không tải). Đây là
ước tính: sụt áp L298N, pin và tải thực tế có thể làm RPM bánh thấp hơn. Phần
trăm trên web là tỷ lệ RPM mục tiêu, không phải PWM thực tế. Lệnh 0% dừng ngay.

Khi có thời gian, tạm đặt một RPM tham chiếu bằng 0 để dùng PWM trực tiếp, kê
xe an toàn, chạy ở 100% và xem `encoder.left.rpm` / `encoder.right.rpm` trong
`/api/status`. Thay hai giá trị `SPEED_PI_*_REFERENCE_RPM` bằng RPM đo riêng
từng bánh, rồi build và flash lại.
Đường kính bánh ảnh hưởng quãng đường, không ảnh hưởng RPM.

PI cập nhật mỗi 100 ms. Các giá trị `SPEED_PI_*_KP` và `SPEED_PI_*_KI` chỉ là
điểm bắt đầu, cần chỉnh trên xe thật: nếu tốc độ dao động, giảm Kp và Ki; nếu
đáp ứng chậm, tăng Kp rồi tăng Ki để giảm sai lệch ổn định. Nếu PWM đã ở 100%
mà RPM đo vẫn thấp hơn mục tiêu, giảm RPM tham chiếu xuống mức xe đạt được. Thử
tốc độ thấp/vừa/cao, thay đổi tải, chạy lùi, xoay, dừng và đảo chiều.
`/api/status` báo RPM mục tiêu, RPM đo, PWM thực tế và `speed_pi_fault`.
Nếu encoder lỗi hoặc bánh đang có lệnh chạy nhưng mất xung trong hai giây, xe
dừng và hủy phiên web. Kiểm tra bánh và cảm biến trước khi kết nối lại. FC-03
chỉ đo xung; chiều quay được suy ra từ lệnh motor.

### IMU MPU6050

Cấu hình I2S simplex hiện tại còn trống GPIO4 và GPIO5 cho IMU. Không dùng GPIO18 vì chân này đã hỏng trên xe. Nối MPU6050 qua I²C như sau:

| Chân MPU6050 | Chân ESP32-S3 | Ghi chú |
| ------------ | ------------- | ------- |
| VCC / VIN    | 3V3           | Dùng 3,3 V để bảo đảm mức logic an toàn |
| GND          | GND chung     | Nối chung mass với ESP32-S3 |
| SDA          | GPIO4         | Dữ liệu I²C |
| SCL          | GPIO5         | Xung nhịp I²C |
| INT          | Để hở         | Firmware đọc cảm biến theo chu kỳ |
| AD0          | GND hoặc để mặc định | Chọn địa chỉ `0x68` |

Điện trở kéo lên SDA/SCL phải nối lên 3,3 V, tuyệt đối không kéo lên 5 V. Nhiều module đã có sẵn điện trở kéo lên. GPIO4/5 thuộc cấu hình I2S duplex thay thế, nên cần đổi chân I²C nếu chuyển chế độ âm thanh. Nếu AD0 nối 3V3, đổi `MPU6050_I2C_ADDRESS` trong [config.h](../config.h) thành `0x69`.

Gắn module chắc chắn và giữ xe đứng yên khoảng hai giây sau khi bật nguồn để
hiệu chuẩn gyro. `gyro_z_dps`, `imu_ready` và `imu_error` cho biết dữ liệu gyro
thô. Firmware hiện không tính heading hoặc dùng IMU để sửa PWM.
Nút xoay trái/phải quay hai motor ngược chiều, không đo góc đích.
## Build và Flash

Dùng ESP-IDF 6.1 (tối thiểu 6.0.1). Đây là lệnh build duy nhất cho board này: màn
ST7789 240×240, L298N, HC-SR04, hai FC-03, MPU6050 và câu đánh thức **“Hi ESP”**. Tên firmware
là `bread-compact-wifi-lcd`.

```sh
python scripts/build.py bread-compact-wifi-lcd --language vi-VN --wake-word wn9_hiesp
```

Sau khi build xong, kết nối ESP32-S3 với máy tính và flash firmware:

```sh
idf.py -p <CONG_SERIAL> flash monitor
```

## Cấu hình

- Để đổi Wi-Fi đã lưu, giữ `BOOT` khoảng 3 giây khi thiết bị đang chạy, sau đó mở
  `http://192.168.4.1` để cấu hình lại.
- Để bật log microphone, đặt `MIC_DEBUG` thành `1` trong `config.h`, rồi build và flash lại firmware.

Dùng trang web tại `http://<IP-robot>:8080` để lái bằng nút giữ hoặc joystick.
Khi tiến/lùi, hai motor nhận cùng phần trăm PWM; xe chưa tự giữ thẳng. Thả nút,
dừng bằng lệnh hoặc mất phiên web 600 ms thì motor dừng. MCP còn
`self.motor.stop` và `self.motor.get_state` để xem PWM
và dữ liệu cảm biến thô. Các lệnh theo cm/góc, follow và né vật cản hiện không hoạt động.
## Cấu hình Telegram

1. Tạo bot với [@BotFather](https://t.me/BotFather), lấy Bot Token.
2. Gửi `/start` cho bot, rồi lấy `result[].message.chat.id` từ:

   ```text
   https://api.telegram.org/bot<TOKEN_CUA_BAN>/getUpdates
   ```

3. Sao chép [telegram_config.example.h](robot/telegram/telegram_config.example.h) thành `robot/telegram/telegram_config.local.h` và điền:

   ```cpp
   #pragma once
   #define L298N_TELEGRAM_ENABLED 1
   #define L298N_TELEGRAM_BOT_TOKEN "TOKEN_CUA_BAN"
   #define L298N_TELEGRAM_CHAT_ID "CHAT_ID_CUA_BAN"
   ```

`telegram_config.local.h` đã được Git ignore. Không chia sẻ file này hoặc firmware có chứa Token.
