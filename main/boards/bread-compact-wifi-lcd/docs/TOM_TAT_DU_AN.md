# Tóm tắt dự án robot XiaoZhi

## Tổng quan

Đây là robot trợ lý giọng nói dùng **ESP32-S3-N16R8**, kết nối Wi-Fi, có màn hình mắt robot, loa, microphone, cảm biến khoảng cách và hệ thống 4 bánh. Firmware hiện tại được cấu hình cho màn hình **ST7789 240×240**, driver motor **L298N**, cảm biến siêu âm **HC-SR04** và hai encoder **FC-03**.

## Chức năng chính

- Điều khiển xe tiến, lùi, xoay trái, xoay phải, dừng và thay đổi tốc độ.
- Chạy tiến/lùi theo quãng đường nhờ xung từ hai encoder bánh xe.
- Chế độ **bám khoảng cách**.
- Chế độ **tự hành né vật cản**.
- Điều khiển thủ công bằng trang web HUD tại `http://<IP-của-robot>:8080`, có joystick, trạng thái cảm biến và cảnh báo vật cản.
- Hiển thị mắt robot và biểu cảm; chuyển động ngắn theo một số cảm xúc của trợ lý.
- Điều khiển và xem trạng thái qua Telegram khi tính năng này được cấu hình.
- Cơ chế an toàn: tự dừng khi mất phiên web, cảm biến/encoder lỗi, hết thời gian chạy hoặc gặp vật cản quá gần khi tiến.

## Linh kiện chính

| Số lượng | Linh kiện                       | Vai trò                                               |
| -------: | ------------------------------- | ----------------------------------------------------- |
|        1 | ESP32-S3-N16R8                  | Bộ điều khiển trung tâm và kết nối Wi-Fi              |
|        1 | Màn hình TFT SPI ST7789 240×240 | Hiển thị mắt robot và trạng thái                      |
|        1 | Microphone I2S                  | Thu âm giọng nói                                      |
|        1 | MAX98357A                       | Khuếch đại âm thanh I2S cho loa                       |
|        1 | Loa                             | Phát giọng nói và âm cảnh báo                         |
|        1 | Driver motor L298N              | Điều khiển chiều quay và PWM của motor                |
|        4 | Động cơ DC                      | Truyền động cho xe                                    |
|        4 | Bánh xe và 1 khung xe           | Phần cơ khí của robot                                 |
|        1 | HC-SR04                         | Đo khoảng cách và phát hiện vật cản phía trước        |
|        2 | FC-03                           | Đếm xung bánh trái/phải, ước lượng RPM và quãng đường |
|        2 | Pin 18650 3,7 V                 | Nguồn cho robot                                       |
|        1 | Công tắc nguồn                  | Bật/tắt nguồn                                         |

> Lưu ý: tất cả module phải nối **chung GND**. Không nối trực tiếp chân ECHO 5 V của HC-SR04 hoặc bất kỳ tín hiệu 5 V nào vào GPIO của ESP32-S3. Nguồn pin 2S nên có BMS 2S và mạch nguồn phù hợp.

## Chân kết nối quan trọng

- Microphone I2S: `WS GPIO1`, `SCK GPIO2`, `SD GPIO17`.
- MAX98357A: `DIN GPIO7`, `BCLK GPIO15`, `LRC GPIO16`.
- Màn hình: `SCL GPIO21`, `SDA GPIO47`, `RES GPIO45`, `DC GPIO40`, `CS GPIO41`, `BLK GPIO42`.
- L298N: motor trái `ENA/IN1/IN2 = GPIO8/9/10`; motor phải `IN3/IN4/ENB = GPIO11/12/13`.
- HC-SR04: `TRIG GPIO14`, `ECHO GPIO39` qua mạch hạ mức.
- FC-03: bánh trái `GPIO38`, bánh phải `GPIO48`.

## Tài liệu liên quan

- Hướng dẫn đầy đủ: [README.vi.md](README.vi.md)
- Cấu hình chân GPIO: [config.h](config.h)
- Điều khiển motor và tự hành: [robot/controller/l298n_controller.cc](robot/controller/l298n_controller.cc)
- Ghi chú phát triển: [update.md](update.md)
