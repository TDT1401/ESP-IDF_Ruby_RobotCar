# Bread Compact Wi-Fi LCD

[English](README.md) | **Tiếng Việt**

Bo mạch ESP32-S3-N16R8, microphone I2S, MAX98357A và màn hình TFT SPI.

## Chức năng

- Trợ lý giọng nói.
- Chế độ follow.
- Chế độ tự hành né vật cản.
- Điều khiển thủ công bằng web với chế độ HUD.
- Điều khiển web tại `http://<IP-robot>:8080`.
- Đo khoảng cách theo thời gian thực.
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
| WS              | GPIO4         |
| SCK             | GPIO5         |
| SD              | GPIO6         |
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

HC-SR04 được bật cùng các biến thể `-l298n`.

| Chân HC-SR04 | Chân ESP32-S3 | Ghi chú                                      |
| ------------ | ------------- | -------------------------------------------- |
| VCC          | 5V            | Không cấp từ 3V3                             |
| GND          | GND           | Phải nối chung GND                           |
| TRIG         | GPIO14        | Tín hiệu ra từ ESP32-S3                      |
| ECHO         | GPIO39        | Bắt buộc hạ mức 5 V xuống 3,3 V trước GPIO39 |

Có thể dùng cầu chia áp cho `ECHO`, ví dụ 1 kΩ từ ECHO đến GPIO39 và 2 kΩ từ GPIO39 xuống GND.
Không nối trực tiếp `ECHO` 5 V vào ESP32-S3.

## Build và Flash

Lệnh build dưới đây chọn câu đánh thức **“Hi ESP”**.

```sh
python scripts/build.py bread-compact-wifi-lcd --name bread-compact-wifi-lcd-st7789-240x240-l298n --wake-word wn9_hiesp
```

Sau khi build xong, kết nối ESP32-S3 với máy tính và flash firmware:

```sh
idf.py -p <CONG_SERIAL> flash monitor
```

## Cấu hình

- Để đổi Wi-Fi đã lưu, giữ `BOOT` khoảng 3 giây khi thiết bị đang chạy, sau đó mở
  `http://192.168.4.1` để cấu hình lại.
- Để bật log microphone, đặt `MIC_DEBUG` thành `1` trong `config.h`, rồi build và flash lại firmware.

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
