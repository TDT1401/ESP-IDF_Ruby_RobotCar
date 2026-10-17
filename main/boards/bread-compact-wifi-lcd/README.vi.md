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
- Telegram.

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
Hiệu chuẩn trong [config.h](config.h): `FC03_LEFT_PULSES_PER_REV` và
`FC03_RIGHT_PULSES_PER_REV` là số xung/vòng bánh; `FC03_LEFT_WHEEL_DIAMETER_MM` và
`FC03_RIGHT_WHEEL_DIAMETER_MM` là đường kính bánh (mm). Hiện cả hai bánh đặt
**22 xung/vòng** và **60 mm**.

## Build và Flash

Dùng ESP-IDF 6.1 (tối thiểu 6.0.1). Đây là lệnh build duy nhất cho board này: màn
ST7789 240×240, L298N, HC-SR04, hai FC-03 và câu đánh thức **“Hi ESP”**. Tên firmware
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
mốc đo, hoặc dừng sớm nếu thiếu xung, lỗi encoder hay quá 10 giây. Khi tiến, xe cũng
dừng nếu HC-SR04 phát hiện vật cản quá gần. `self.motor.get_state`
và `/api/status` trả `distance_move_state`, `distance_move_target_cm` và
`distance_move_progress_cm`. Lệnh trả về ngay khi bắt đầu; `completed` mới là đã đạt mốc.
Số đo có thể lệch quãng đường thực do trượt bánh và sai số hiệu chuẩn.

## Cấu hình Telegram

1. Tạo bot với [@BotFather](https://t.me/BotFather), lấy Bot Token.
2. Gửi `/start` cho bot, rồi lấy `result[].message.chat.id` từ:

   ```text
   https://api.telegram.org/bot<TOKEN_CUA_BAN>/getUpdates
   ```

3. Sao chép [telegram_config.example.h](l298n/telegram_config.example.h) thành `l298n/telegram_config.local.h` và điền:

   ```cpp
   #pragma once
   #define L298N_TELEGRAM_ENABLED 1
   #define L298N_TELEGRAM_BOT_TOKEN "TOKEN_CUA_BAN"
   #define L298N_TELEGRAM_CHAT_ID "CHAT_ID_CUA_BAN"
   ```

`telegram_config.local.h` đã được Git ignore. Không chia sẻ file này hoặc firmware có chứa Token.
