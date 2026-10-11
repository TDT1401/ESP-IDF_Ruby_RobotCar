# Bread Compact Wi-Fi LCD

Board ESP32-S3-N16R8, microphone I2S, MAX98357A và màn hình TFT SPI.

## Chức năng

- Trợ lý giọng nói.
- Mắt robot vector dùng engine [RobotEye](https://github.com/fonzu/RobotEye), với morph,
  chớp mắt và chuyển hướng nhìn không chặn luồng chính. Bản tích hợp dùng backend LVGL cho TFT
  thay cho backend Arduino/U8g2 của thư viện gốc.
- Giao diện nhiều màn hình tự nhận biết câu hỏi: pin, đồng hồ, trạng thái Wi-Fi và nhiệt độ;
  trang thông tin tự quay lại mắt robot sau 15 giây.
- Điều khiển motor, follow và tự hành né vật cản bằng giọng nói/MCP; điều khiển thủ công bằng web.
- Telegram.
- Điều khiển web tại `http://<IP-robot>:8080`: tiến, lùi, trái, phải, dừng, tốc độ PWM
  và khoảng cách HC-SR04 theo thời gian thực.

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

HC-SR04 được bật cùng các biến thể `-l298n`:

| Chân HC-SR04 | Chân ESP32-S3 | Ghi chú                                      |
| ------------ | ------------- | -------------------------------------------- |
| VCC          | 5V            | Không cấp từ 3V3                             |
| GND          | GND           | Phải nối chung GND                           |
| TRIG         | GPIO14        | Tín hiệu ra từ ESP32-S3                      |
| ECHO         | GPIO39        | Bắt buộc hạ mức 5 V xuống 3,3 V trước GPIO39 |

Có thể dùng cầu chia áp cho `ECHO`, ví dụ 1 kΩ từ ECHO đến GPIO39 và 2 kΩ từ GPIO39 xuống GND.
Không nối trực tiếp `ECHO` 5 V vào ESP32-S3.

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
| `/status`        | Xem robot, màn hình, motor, loa, mic và SR04. |
| `/web`           | Lấy link trang điều khiển web hiện tại.     |
| `/wake`          | Đánh thức robot khi robot đang chờ.         |
| `/speed [0-100]` | Xem hoặc đặt tốc độ PWM; `0` dừng motor.    |
| `/follow on|off` | Bật hoặc tắt follow mode.                   |
| `/follow N`      | Đặt khoảng cách follow N cm và bật mode.    |
| `/avoid on|off`  | Bật hoặc tắt tự hành né vật cản.            |
| `/avoid N`       | Đặt khoảng cách né vật cản N cm và bật mode. |
| `/stop`          | Dừng motor và tắt mọi chế độ tự hành.       |
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
| Khoảng cách follow mặc định | 30 cm        |
| Khoảng cách né vật cản      | 20 cm        |

Robot không có encoder. Một HC-SR04 cố định phía trước chỉ bám khoảng cách với vật thể nằm trong
chùm siêu âm, không nhận diện người và không xác định được hướng trái/phải của mục tiêu. Follow mode
chỉ tự chạy tiến và dừng, không tự lùi. Chế độ né vật cản tiến chậm, xoay tại chỗ để tìm hướng trống
và dừng nếu cả hai lần thử vẫn bị chặn.

Có thể điều khiển bằng giọng nói, ví dụ:

- `Bật chế độ đi theo`, `Tắt follow mode`.
- `Đặt khoảng cách đi theo là 40 cm`.
- `Bật tự hành né vật cản`, `Tắt chế độ né vật cản`.
- `Đặt khoảng cách né vật cản là 25 cm`.
- `Dừng xe`.

Khoảng cách follow hợp lệ là 10–200 cm; khoảng cách né vật cản hợp lệ là 10–100 cm. Các giá trị và
mode chỉ lưu trong RAM, trở về 30 cm, 20 cm và chế độ thủ công sau khi khởi động lại. Mất Echo, dữ
liệu cảm biến quá hạn, lỗi motor, lệnh dừng hoặc biểu cảm `sleep/sleepy` đều tắt chế độ tự hành và
làm motor dừng.

### Dùng web

1. Kết nối robot và điện thoại/máy tính vào cùng Wi-Fi.
2. Mở URL `L298nWeb: Motor control: http://<IP>:8080` trong serial log.
3. Bấm **Bắt đầu**, giữ nút điều hướng để chạy và thả để dừng.
4. Bấm **Trả về giọng nói** khi hoàn tất. Khi web giành quyền, follow/né vật cản sẽ bị tắt.
5. Dùng các tab **Né vật cản** và **Follow** để bật hoặc tắt từng chế độ. Khi bật tự hành,
   trang web tự trả quyền điều khiển thủ công trước; hai chế độ tự hành loại trừ nhau.

Thẻ HC-SR04 trên web cập nhật khoảng cách, chế độ tự hành và hai ngưỡng follow/né vật cản mỗi
2 giây. Khi không nhận được Echo, thẻ hiển thị lỗi và không giữ lại số đo cũ.
Giao diện mặc định dùng chủ đề tối kiểu cyber cockpit và tự chuyển bố cục cho màn hình điện thoại.

Trong buồng lái ngang, heartbeat vẫn duy trì phiên khi không bấm nút. Mất focus tạm thời
khi trang còn hiển thị sẽ dừng xe nhưng giữ phiên; chuyển ứng dụng, ẩn hoặc đóng trang sẽ trả quyền.
Một lỗi truyền mạng được thử lại một lần bằng lệnh **Dừng** với số thứ tự mới, không tự chạy lại
theo hướng cũ. Watchdog motor vẫn giữ giới hạn 600 ms. Nếu phiên đã hết hoặc bị thu hồi,
bấm **Kết nối lại** ngay trên HUD để nhận phiên mới; không cần thoát màn hình ngang.
Trang không tự nhận lại phiên đã bị thu hồi, vì lệnh dừng bằng giọng nói cũng có thể hủy phiên.

Buồng lái yêu cầu giữ sáng màn hình nếu trình duyệt hỗ trợ Screen Wake Lock. Tính năng này cần
secure context; URL HTTP nội bộ của ESP32 thường không hỗ trợ. Khi HUD nhắc về tự tắt màn hình,
hãy tăng thời gian chờ màn hình trong cài đặt điện thoại. Không chạm màn hình không làm hết phiên
nếu trình duyệt vẫn chạy heartbeat; khóa màn hình có thể làm trình duyệt ngừng gửi heartbeat.

Chỉ dùng trong mạng nội bộ; trang web không có đăng nhập. Khi thử lần đầu, kê bánh xe khỏi mặt đất.
Lệnh **Dừng**, mất kết nối hoặc đóng trang phải làm motor dừng; hết phiên hoặc đóng trang sẽ trả quyền
điều khiển về giọng nói.

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

Sau khi robot có Wi-Fi, bot phải gửi thông báo khởi động. Thử `/status`, `/web`, `/wake`, `/speed 50`,
`/follow 30`, `/avoid 20` và `/stop`.

`/status` báo trạng thái firmware của màn hình, motor, loa, mic và HC-SR04; HC-SR04 kèm khoảng cách
đo hiện tại. Nếu không có đối tượng thiết bị, thông số không hợp lệ hoặc SR04 không nhận Echo, mục
tương ứng sẽ báo lỗi. Màn hình, L298N, MAX98357A và mic I2S không có đường nhận dạng thiết bị nên
trạng thái chỉ xác nhận phía firmware, không thể xác nhận dây nối hoặc tải vật lý thực tế.

Để gửi nội dung do XiaoZhi trả lời hoặc tìm được, hãy yêu cầu rõ trong câu nói, ví dụ:

- `Tìm dự báo thời tiết ngày mai và gửi kết quả qua Telegram.`
- `Gửi phần tóm tắt vừa rồi cùng các đường dẫn qua Telegram.`

Firmware cung cấp MCP tool `self.telegram.send_message` cho yêu cầu này. Tool chỉ được AI gọi khi người dùng nói rõ muốn gửi qua Telegram, gửi văn bản thuần tối đa 4000 byte và đưa vào hàng đợi hữu hạn để không chặn tác vụ chính hoặc âm thanh. Kết quả trả về xác nhận nội dung đã được xếp hàng, không phải biên nhận giao hàng từ Telegram.

Nếu không hoạt động: kiểm tra Token/Chat ID, Internet/DNS/thời gian hệ thống và bảo đảm không có chương trình khác polling hoặc webhook dùng cùng bot.
