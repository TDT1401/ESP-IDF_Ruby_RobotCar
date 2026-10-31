# Bread Compact Wi-Fi LCD

[English](README.md) | **Tiếng Việt**

Bo mạch ESP32-S3-N16R8, microphone I2S, MAX98357A và màn hình TFT SPI.

## Chức năng

- Trợ lý giọng nói.
- Chế độ follow.
- Chế độ tự hành né vật cản.
- Điều khiển thủ công bằng web với chế độ HUD.
- Điều khiển web tại `http://<IP-robot>:8080`.
- Đo khoảng cách vật cản theo thời gian thực.
- Hai FC-03: đếm xung, đo RPM và cho phép xe tiến/lùi theo quãng đường.
- MPU6050 cung cấp phản hồi hướng bắt buộc khi chạy theo quãng đường và quay theo góc.
- Telegram.

## Cấu trúc mã nguồn

Các chức năng robot được nhóm trong `robot/`:

```text
robot/
├── controller/  # Đầu ra bánh xe, chuyển động đo được, tự hành và công cụ MCP
├── sensors/     # Encoder FC-03, HC-SR04 và MPU6050
├── web/         # HTTP server và trang điều khiển nhúng
├── telegram/    # Tích hợp Telegram, nội dung tin nhắn và cấu hình cá nhân
└── tests/       # Kiểm thử hồi quy trang điều khiển
```

`compact_wifi_board_lcd.cc` khởi tạo các module này. Chân GPIO vẫn nằm trong
`config.h`; `main/CMakeLists.txt` chọn các file nguồn robot khi bật
`CONFIG_BREAD_COMPACT_WIFI_LCD_L298N`.

Bộ điều khiển dùng chung một trạng thái trong `l298n_internal.h`. Phần triển khai
được chia theo chức năng: `l298n_motor_driver.cc` xuất tín hiệu bánh xe,
`l298n_motion_executor.cc` kết hợp encoder và IMU; bám theo và né vật cản nằm
trong các file `l298n_follow.cc` và `l298n_obstacle_avoidance.cc`. Lệnh web và
MCP có file nguồn riêng.

Chạy kiểm thử trang điều khiển từ thư mục gốc dự án:

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
**20,3 xung/vòng** bên trái, **20,8 xung/vòng** bên phải và **60 mm**.

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

Gắn module nằm ngang, cố định chắc gần giữa khung xe, mặt linh kiện hướng lên.
Giữ xe đứng yên khoảng hai giây sau khi bật nguồn để hiệu chuẩn gyro. Firmware đọc gyro Z
trong task riêng, kết hợp với quãng đường có dấu từ hai FC-03 để ước lượng hướng, bù PWM
hai bên khi chạy thẳng và dùng góc đo được để phanh lượt quay đúng lúc.

Chạy theo quãng đường và quay theo góc yêu cầu cả encoder đã hiệu chuẩn lẫn IMU sẵn sàng.
Nếu một cảm biến mất dữ liệu trong lúc chạy, xe dừng thay vì đổi cách ước lượng.
Xem `imu_ready`, `imu_error`, `imu_axis_mismatch`,
`heading_degrees`, `gyro_z_dps`, `heading_hold_active` và `turn_uses_imu` tại `/api/status`
hoặc `self.motor.get_state`. `heading_degrees` là góc tương đối từ lúc khởi động;
`turn_uses_imu` cho biết lượt quay dùng IMU đã bắt đầu hay đang hoạt động. Khi xe rẽ trái,
`heading_degrees` phải tăng; nếu trục Z ngược,
xe dừng quay và đặt `imu_axis_mismatch`. Khi đó kiểm tra chiều gắn module và
`MPU6050_GYRO_Z_SIGN` trong `config.h`.

Trong Serial Monitor, lọc tag `MPU6050`. Lúc khởi động, firmware in chân SDA/SCL, kết quả
thăm dò hai địa chỉ `0x68` và `0x69`, rồi giá trị thanh ghi `WHO_AM_I`. Firmware chấp nhận
`0x68` của MPU-6050 và `0x70` của IMU tương thích MPU-6500; nếu module ghi MPU6050 nhưng
trả `0x70`, monitor sẽ báo rõ và vẫn đọc gyro Z theo các thanh ghi tương thích.
Sau đó monitor in kết quả hiệu chuẩn. Nếu cả hai địa chỉ không phản hồi, kiểm tra nguồn 3V3, GND chung,
thứ tự SDA/SCL và điện trở kéo lên. Nếu chỉ `0x69` phản hồi, AD0 đang ở mức cao: đổi
`MPU6050_I2C_ADDRESS` thành `0x69` hoặc kéo AD0 xuống GND. Sau khi sẵn sàng, log in tốc độ
quay Z mỗi 5 giây. Lỗi đọc I²C rời rạc được tổng hợp theo 5 giây; nếu lỗi liên tiếp từ
5 lần, monitor báo ngay. Nếu mất mẫu quá lâu khi đang quay bằng IMU, xe sẽ dừng.

Không lắp đèn và GPIO18 không dùng được, nên các công cụ MCP điều khiển đèn đã bị tắt.
Các lượt quay theo góc dùng IMU giữ tốc độ quay đã chọn cho đến 25° cuối, rồi giảm dần
PWM xuống tối thiểu 55% gần góc đích. Nếu motor kêu mà không quay ở mức này, kiểm tra
nguồn motor, dây, L298N và bánh xe trước khi thử tiếp.
Điện trở kéo lên SDA/SCL chỉ phục vụ giao tiếp I²C: thiếu chúng có thể làm mất mẫu IMU,
nhưng không trực tiếp làm motor thiếu lực quay. Dừng thử nếu motor chỉ kêu mà không quay.
Cần chạy thử trên sàn để chỉnh `kTurnSlowdownWindowDegrees`, `kTurnStopLeadSeconds`,
`kTurnKp`, `kTurnKd`, `kHeadingKp`
và `kHeadingKd` trong `robot/controller/l298n_controller.cc`.

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

Để xe tiến 50 cm, nói “tiến 50 cm” hoặc gọi MCP `self.motor.forward_distance` với
`distance_cm=50`; lùi dùng `self.motor.backward_distance`. Xe dừng khi cả hai bánh đạt
mốc đo, hoặc dừng sớm nếu thiếu xung, lỗi cảm biến hay quá hạn an toàn theo quãng đường.
Khi tiến, xe cũng dừng nếu HC-SR04 phát hiện vật cản quá gần. `self.motor.get_state`
và `/api/status` trả `distance_move_state`, `distance_move_target_cm` và
`distance_move_progress_cm`. Lệnh trả về ngay khi bắt đầu; `completed` mới là đã đạt mốc.
Số đo có thể lệch quãng đường thực do trượt bánh và sai số hiệu chuẩn.
Các lệnh MCP ngắn `self.motor.forward` và `self.motor.backward` cũng chạy theo
quãng đường đo; `self.motor.left` và `self.motor.right` quay theo góc đo.
Đã bỏ cử chỉ motor chạy theo thời gian. Nút điều khiển web và joystick vẫn chạy
trong lúc giữ lệnh, với cơ chế tự dừng khi mất phiên 600 ms.
Lệnh chạy theo quãng đường bắt đầu từ mức PWM tối thiểu 50%, rồi giảm tốc trong 8 cm cuối.
Quãng đường ngắn dùng profile tăng giảm tốc hình tam giác.

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
