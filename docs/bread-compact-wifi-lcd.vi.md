# Bread Compact Wi-Fi LCD — Hướng dẫn nối dây

[English](../main/boards/bread-compact-wifi-lcd/BOARD_GUIDE.md)

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

## Màn hình TFT SPI (ST7789 240×240)

Bản build mặc định đang tạm tắt màn hình. Firmware không khởi tạo SPI/TFT hay đèn nền; GPIO21, GPIO40, GPIO41, GPIO42, GPIO45 và GPIO47 không bị phần màn hình cấu hình, để dành cho cảm biến sau này. Giữ lại sơ đồ dây dưới đây để lắp lại màn hình. Chạy `idf.py reconfigure` trước khi build trực tiếp để `sdkconfig` cũ nhận cờ màn hình mới. Muốn bật lại, đặt `CONFIG_BREAD_COMPACT_WIFI_LCD_DISPLAY=y` trong `config.json` cho bản build chuẩn, hoặc bật tùy chọn này trong `idf.py menuconfig`.

| Chân màn hình | Nối với |
| ------------- | ------- |
| GND           | GND     |
| VCC           | 3V3     |
| SCL           |         |
| SDA           |         |
| RES           |         |
| DC            |         |
| CS            |         |
| BLK           |         |

Chân SCL và SDA trên màn hình mang xung nhịp và dữ liệu SPI.

## Driver motor L298N

| Chân L298N  | Nối với                                                                 |
| ----------- | ----------------------------------------------------------------------- |
| ENA         | GPIO8                                                                   |
| IN1         | GPIO9                                                                   |
| IN2         | GPIO10                                                                  |
| IN3         | GPIO11                                                                  |
| IN4         | GPIO12                                                                  |
| ENB         | GPIO13                                                                  |
| OUT1 / OUT2 | Hai motor bên trái, mắc song song                                       |
| OUT3 / OUT4 | Hai motor bên phải, mắc song song                                       |
| VS / +12V   | Cực dương của nguồn motor riêng, có điện áp phù hợp với motor và driver |
| GND         | Cực âm nguồn motor và GND của ESP32-S3                                  |

Tháo jumper ENA và ENB. Khuyến nghị kéo mỗi chân ENA và ENB xuống GND bằng điện trở 10 kΩ. Không cấp nguồn motor từ đường 3V3 hoặc 5V của ESP32-S3.

## Cảm biến siêu âm HC-SR04

| Chân HC-SR04 | Nối với                                    |
| ------------ | ------------------------------------------ |
| VCC          | Nguồn 5V ổn áp                             |
| GND          | GND                                        |
| TRIG         | GPIO14                                     |
| ECHO         | GPIO39 qua mạch chia áp từ 5 V xuống 3,3 V |

Để chia áp cho ECHO, nối điện trở 1 kΩ từ ECHO đến GPIO39 và 2 kΩ từ GPIO39 xuống GND. Không nối trực tiếp tín hiệu ECHO 5 V vào GPIO39.

## Servo HC-SR04 — giữ góc giữa

| Dây servo                 | Nối với                                      |
| ------------------------- | -------------------------------------------- |
| Tín hiệu (cam/vàng/trắng) | GPIO6                                        |
| V+ (đỏ)                   | Nguồn 5 V ổn áp đủ dòng khi servo bị giữ tải |
| GND (nâu/đen)             | GND nguồn, nối chung GND ESP32-S3            |

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
