# Ruby RobotCar — Hướng dẫn nối dây

[English](BOARD_GUIDE.md)

Sơ đồ chân này dành cho board ESP32-S3-N16R8. Ngắt nguồn trước khi nối dây. Nối GND chung giữa ESP32-S3, các module và nguồn motor. Cấp đúng điện áp ghi cho từng module; không đưa tín hiệu 5 V vào GPIO của ESP32-S3.

## Microphone I2S

| Chân microphone | Nối với |
| --------------- | ------- |
| VDD             | 3V3     |
| GND             | GND     |
| WS              | GPIO1   |
| SCK             | GPIO2   |
| SD              | GPIO17  |
| L/R             | GND     |

## Mạch khuếch đại MAX98357A và loa

| Chân mạch khuếch đại | Nối với        |
| -------------------- | -------------- |
| VIN                  | Nguồn 5V ổn áp |
| GND                  | GND            |
| DIN                  | GPIO7          |
| BCLK                 | GPIO15         |
| LRC                  | GPIO16         |
| + / -                | Hai dây loa    |
| SD                   | Để hở          |
| GAIN                 | Để hở          |

Nối loa vào hai ngõ ra + và - của mạch khuếch đại, không nối vào GND.

## Màn hình

Firmware không dùng màn hình và không khởi tạo SPI/TFT hay đèn nền.
Không nối TFT vào GPIO21, GPIO40, GPIO41, GPIO42, GPIO45 hoặc GPIO47 vì các chân này
được dùng cho L298N phải. GPIO18 vẫn để trống.

## Hai L298N và bánh thường

Xe dùng bốn bánh thường, điều khiển vi sai trái/phải. Bánh trước ở đầu xe, bánh sau ở đuôi xe. Mỗi kênh L298N điều khiển riêng một motor. Giữ audio, IMU, hai encoder và các chân USB/UART hiện tại.

| Chân điều khiển | L298N trái | L298N phải |
| --------------- | ---------- | ---------- |
| ENA — PWM       | GPIO8      | GPIO45     |
| IN1             | GPIO9      | GPIO21     |
| IN2             | GPIO10     | GPIO40     |
| IN3             | GPIO11     | GPIO41     |
| IN4             | GPIO12     | GPIO42     |
| ENB — PWM       | GPIO13     | GPIO47     |

GPIO45 nối ENA của L298N phải là chân strapping chọn điện áp VDD_SPI lúc reset. Tháo jumper ENA và nối điện trở 10 kΩ từ GPIO45/ENA xuống GND để giữ mức thấp lúc khởi động; không kéo chân này lên 5 V. Sơ đồ này thay cho ENA phải dùng GPIO18 trước đây.

| Ngõ ra              | L298N trái    | L298N phải     |
| ------------------- | ------------- | -------------- |
| OUT1 / OUT2 — kênh A | Bánh sau trái | Bánh trước phải |
| OUT3 / OUT4 — kênh B | Bánh trước trái | Bánh sau phải |

Tháo cả bốn jumper ENA/ENB trên hai module để ESP32 cấp bốn tín hiệu PWM độc lập. Khuyến nghị điện trở kéo xuống 10 kΩ tại mỗi chân ENA/ENB. Nối GND chung giữa ESP32, hai module và nguồn motor riêng. Không cấp nguồn motor từ đường 3V3 hoặc 5V của ESP32-S3. Hai encoder FC-03 gắn ở bánh trước: trái dùng GPIO38, phải dùng GPIO48; hai bánh sau chưa có encoder.

Bản build duy nhất giữ tên firmware/OTA hiện có trong `config/build_config.json` và dùng bốn bánh thường với hai L298N khi bật `CONFIG_BREAD_COMPACT_WIFI_LCD_L298N=y`. Target vẫn là ESP32-S3, flash 16 MB và partition `partitions/v2/16m.csv` theo defaults của dự án. Khi tự build trong môi trường ESP-IDF, dùng:

```sh
python scripts/build.py
```

Giữ các tùy chọn đảo chiều có sẵn `CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_LEFT/RIGHT` cho bánh trước. Hai bánh sau dùng chiều mặc định theo sơ đồ dây, không có tùy chọn build mới. Cần kiểm tra chiều từng bánh trên xe và hiệu chỉnh lại đường kính bánh, CPR và gains theo bánh thường thực tế. Code chưa được build/test hoặc kiểm chứng phần cứng trong lần cập nhật này.

## Cảm biến siêu âm HC-SR04

Gắn HC-SR04 cố định hướng về phía trước. GPIO6 để trống.

| Chân HC-SR04 | Nối với                                    |
| ------------ | ------------------------------------------ |
| VCC          | Nguồn 5V ổn áp                             |
| GND          | GND                                        |
| TRIG         | GPIO14                                     |
| ECHO         | GPIO39 qua mạch chia áp từ 5 V xuống 3,3 V |

Để chia áp cho ECHO, nối điện trở 1 kΩ từ ECHO đến GPIO39 và 2 kΩ từ GPIO39 xuống GND. Không nối trực tiếp tín hiệu ECHO 5 V vào GPIO39.

## Cảm biến bánh xe FC-03

| Chân FC-03 | Cảm biến trái | Cảm biến phải |
| ---------- | ------------- | ------------- |
| VCC        | 3V3           | 3V3           |
| GND        | GND           | GND           |
| D0 / DO    | GPIO38        | GPIO48        |
| A0 / AO    | Để hở         | Để hở         |

Ngõ ra D0/DO phải dùng mức logic 3,3 V.

## IMU MPU6050

| Chân MPU6050 | Nối với                |
| ------------ | ---------------------- |
| VCC / VIN    | 3V3                    |
| GND          | GND                    |
| SDA          | GPIO4                  |
| SCL          | GPIO5                  |
| INT          | Để hở                  |
| AD0          | GND (địa chỉ I²C 0x68) |

Điện trở kéo lên của SDA và SCL phải nối lên 3,3 V, không nối lên 5 V.
