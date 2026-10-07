# Bread Compact Wi-Fi LCD

Board ESP32-S3-N16R8, microphone I2S, MAX98357A và màn hình TFT SPI.

## Chức năng

- Trợ lý giọng nói.
- Mắt robot vector dùng engine [RobotEye](https://github.com/fonzu/RobotEye), với morph,
  chớp mắt và chuyển hướng nhìn không chặn luồng chính. Bản tích hợp dùng backend LVGL cho TFT
  thay cho backend Arduino/U8g2 của thư viện gốc.
- Giao diện nhiều màn hình tự nhận biết câu hỏi: pin, đồng hồ, trạng thái Wi-Fi và nhiệt độ;
  trang thông tin tự quay lại mắt robot sau 15 giây.
- Điều khiển motor bằng giọng nói/MCP, trang web nội bộ.
- Telegram.
- Điều khiển web tại `http://<IP-robot>:8080`: tiến, lùi, trái, phải, dừng và tốc độ PWM.

## Nối dây

Tất cả module phải dùng chung GND.

| Chân module    | Chân ESP32-S3 |
| -------------- | ------------- |
| Microphone VDD | 3V3           |
| Microphone GND | GND           |
| Microphone WS  | GPIO4         |
| Microphone SCK | GPIO5         |
| Microphone SD  | GPIO6         |
| Microphone L/R | GND           |
| MAX98357A VIN  | 5V            |
| MAX98357A GND  | GND           |
| MAX98357A DIN  | GPIO7         |
| MAX98357A BCLK | GPIO15        |
| MAX98357A LRC  | GPIO16        |
| TFT GND        | GND           |
| TFT VCC        | 3V3           |
| TFT SCL        | GPIO21        |
| TFT SDA        | GPIO47        |
| TFT RES        | GPIO45        |
| TFT DC         | GPIO40        |
| TFT CS         | GPIO41        |
| TFT BLK        | GPIO42        |

Loa chỉ nối vào hai cực `+`/`-` của MAX98357A. Để hở `SD` và `GAIN` của MAX98357A.

Với biến thể `-l298n`:

| Kênh  | Chân L298N | Chân ESP32-S3      | Chức năng        |
| ----- | ---------- | ------------------ | ---------------- |
| Trái  | ENA        | GPIO8              | PWM motor trái   |
| Trái  | IN1        | GPIO9              | Chiều motor trái |
| Trái  | IN2        | GPIO10             | Chiều motor trái |
| Phải  | IN3        | GPIO11             | Chiều motor phải |
| Phải  | IN4        | GPIO12             | Chiều motor phải |
| Phải  | ENB        | GPIO13             | PWM motor phải   |
| Chung | GND        | GND                | Mass nguồn motor |
| Trái  | OUT1, OUT2 | Hai dây motor trái | -                |
| Phải  | OUT3, OUT4 | Hai dây motor phải | -                |

- Tháo jumper `ENA`/`ENB`; không đưa 5 V vào GPIO.
- Cấp nguồn motor riêng qua L298N và nối chung GND.
- Khuyến nghị kéo `ENA` và `ENB` xuống GND bằng điện trở 10 kΩ.

## Build

```sh
python scripts/build.py bread-compact-wifi-lcd --name bread-compact-wifi-lcd-st7789-240x240-l298n --wake-word wn9_hiesp
```

## Flash

```sh
idf.py -p <CONG_SERIAL> flash monitor
```

## Hướng dẫn sử dụng

- Câu đánh thức: `Hi ESP`.
- Có thể hỏi `Pin còn bao nhiêu?`, `Mấy giờ rồi?`, `Wi-Fi đang kết nối không?`,
  `Nhiệt độ bao nhiêu?` hoặc `Thời tiết hôm nay thế nào?` để mở trang tương ứng. Giờ và Wi-Fi
  lấy từ thiết bị. Pin/nhiệt độ ưu tiên cảm biến của board; nếu board không có cảm biến, màn hình
  lấy số từ câu trả lời của trợ lý (`78%`, `78 phần trăm`, `28°C`, `28 độ C`) và hiển thị `--`
  khi câu trả lời không có dữ liệu. Trang `WEATHER` tự nhận biết nội dung trợ lý đang đọc và vẽ
  cảnh nắng, mây, mưa, giông, sương mù hoặc tuyết. Mưa, tuyết và sương mù có chuyển động; phần
  này được vẽ trực tiếp bằng LVGL nên không cần thư viện thời tiết bổ sung.
- Giữ `BOOT` 3 giây khi thiết bị đang chạy để xóa Wi-Fi đã lưu, sau đó vào `http://192.168.4.1` để cấu hình lại.
- Đặt `MIC_DEBUG` thành `1` trong `config.h` nếu cần log microphone, rồi build và flash lại.
- Lệnh Telegram:

| Lệnh             | Mô tả                                       |
| ---------------- | ------------------------------------------- |
| `/status`        | Xem trạng thái robot và motor.              |
| `/web`           | Lấy link trang điều khiển web hiện tại.     |
| `/wake`          | Đánh thức robot khi robot đang chờ.         |
| `/speed [0-100]` | Xem hoặc đặt tốc độ PWM; `0` dừng motor.    |
| `/start`         | Khởi động bot và hiển thị hướng dẫn cơ bản. |
| `/menu`          | Mở menu lệnh Telegram.                      |
| `/help`          | Hiển thị danh sách lệnh trợ giúp.           |

## Motor L298N

Áp dụng cho các biến thể có hậu tố `-l298n`.

### Tham số

| Tham số                     | Giá trị      |
| --------------------------- | ------------ |
| `duration_ms`               | 100–10000 ms |
| `speed_percent`             | 0–100        |
| Tốc độ mặc định             | 75%          |
| Thời gian mặc định tiến/lùi | 1000 ms      |
| Thời gian mặc định xoay     | 400 ms       |

Robot không có encoder, cảm biến tránh vật cản hoặc đo quãng đường.

### Dùng web

1. Kết nối robot và điện thoại/máy tính vào cùng Wi-Fi.
2. Mở URL `L298nWeb: Motor control: http://<IP>:8080` trong serial log.
3. Bấm **Bắt đầu**, giữ nút điều hướng để chạy và thả để dừng.
4. Bấm **Trả về giọng nói** khi hoàn tất.

Chỉ dùng trong mạng nội bộ; trang web không có đăng nhập. Khi thử lần đầu, kê bánh xe khỏi mặt đất. Lệnh **Dừng**, mất kết nối hoặc đóng trang phải làm motor dừng; hết phiên hoặc đóng trang sẽ trả quyền điều khiển về giọng nói.

## Telegram cho L298N

Áp dụng cho firmware L298N.

Telegram cần Internet, thời gian hệ thống hợp lệ và CA bundle của ESP-IDF. Link web chỉ truy cập được trong cùng mạng với robot.

### Cấu hình

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

### Kiểm tra

Sau khi robot có Wi-Fi, bot phải gửi thông báo khởi động. Thử `/status`, `/web`, `/wake` và `/speed 50`.

Để gửi nội dung do XiaoZhi trả lời hoặc tìm được, hãy yêu cầu rõ trong câu nói, ví dụ:

- `Tìm dự báo thời tiết ngày mai và gửi kết quả qua Telegram.`
- `Gửi phần tóm tắt vừa rồi cùng các đường dẫn qua Telegram.`

Firmware cung cấp MCP tool `self.telegram.send_message` cho yêu cầu này. Tool chỉ được AI gọi khi người dùng nói rõ muốn gửi qua Telegram, gửi văn bản thuần tối đa 4000 byte và đưa vào hàng đợi hữu hạn để không chặn tác vụ chính hoặc âm thanh. Kết quả trả về xác nhận nội dung đã được xếp hàng, không phải biên nhận giao hàng từ Telegram.

Nếu không hoạt động: kiểm tra Token/Chat ID, Internet/DNS/thời gian hệ thống và bảo đảm không có chương trình khác polling hoặc webhook dùng cùng bot.
