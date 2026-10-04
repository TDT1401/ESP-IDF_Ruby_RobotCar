# Telegram cho robot L298N

Áp dụng cho ba biến thể `bread-compact-wifi-lcd` có hậu tố `-l298n`. Telegram
chạy song song với trợ lý giọng nói và trang web điều khiển xe.

## Chức năng hiện có

- Sau mỗi lần khởi động, khi có Wi-Fi, Internet và giờ hệ thống hợp lệ, bot
  gửi thông báo robot đã khởi động, phiên bản firmware và link web
  `http://<IP-robot>:8080`. Gửi lại nếu lần gửi thất bại, có thời gian chờ giữa
  các lần thử. Mất mạng rồi nối lại cùng IP không gửi lại thông báo khởi động;
  khi IP thay đổi sẽ gửi link mới.
- `/status`: trạng thái ứng dụng (chờ, nghe, nói…), uptime, phiên bản firmware,
  Wi-Fi/RSSI, heap trống, chế độ motor web/giọng nói, hướng lệnh, chạy/chờ/dừng,
  PWM đang đặt, tốc độ giọng nói, bật/tắt chuyển động cảm xúc và lỗi GPIO/PWM.
- `/web`: lấy link web hiện tại.
- `/wake`: đánh thức robot đang chờ để trò chuyện qua mic trên robot. Nếu đang
  nghe/nói/kết nối, chỉ báo trạng thái, không tắt cuộc trò chuyện.
- `/speed`: xem tốc độ đã đặt và PWM đang chạy; `/speed 50`: đặt PWM 50%.
  Nhận đúng một số nguyên 0–100, có nút chọn nhanh 25%, 50%, 75%.
  Dùng chung tốc độ với lệnh giọng nói: áp dụng ngay khi chạy, không kéo dài
  thời gian chạy và không tự khởi động motor đang dừng. `/speed 0` dừng motor.
  Việc đặt tốc độ hủy chuyển động cảm xúc hiện tại như lệnh đặt tốc độ bằng giọng nói.
  Nếu web đang giữ quyền, lệnh bị từ chối (kể cả mức 0); bấm **Trả về giọng nói**
  trên web trước. Không thay đổi thanh tốc độ riêng của web.
  Cài đặt chỉ giữ trong RAM, khởi động lại trở về mức mặc định 75%.
- `/start`, `/menu`, `/help`: trợ giúp và bàn phím Telegram chứa các lệnh trên.
- Nhận lệnh trong chat riêng hoặc nhóm có đúng Chat ID cấu hình. Lệnh gửi kèm
  tên bot, ví dụ `/status@TenBotCuaBan`, chỉ được xử lý nếu đúng bot này.

Trạng thái motor là **lệnh/PWM của firmware**, không phải phản hồi encoder.
Robot này chưa có cảm biến khoảng cách, theo người hay tránh vật cản, nên
không báo các mục đó như mẫu Kylio. Telegram có thể đặt tốc độ và dừng bằng mức 0
ở chế độ giọng nói; không ra lệnh tiến/lùi/xoay hoặc giành quyền điều khiển web.

Telegram cần Internet trên mạng/hotspot cấp cho robot. Link web là **IP nội bộ**,
không phải đường hầm truy cập từ xa. Người mở link cần truy cập được cùng mạng
với robot; có thể cần chọn “Mở trong trình duyệt” từ Telegram. Nếu iPhone đang
giữ quyền điều khiển web, bấm **Trả về giọng nói** trước khi Android nhận quyền.

## Chỉnh nội dung tin nhắn

Toàn bộ câu chữ, tiêu đề, biểu tượng và bàn phím lệnh Telegram nằm trong
[`l298n/l298n_telegram_text.h`](l298n/l298n_telegram_text.h). Chỉ cần sửa các hằng chuỗi
trong file này; phần kết nối Telegram và thu thập trạng thái vẫn nằm trong
`l298n/l298n_telegram.cc`.

Tin nhắn sử dụng định dạng HTML của Telegram. Có thể dùng các thẻ `<b>`, `<i>`, `<code>`
và `<a>`, nhưng phải đóng đủ thẻ. Không đổi tên các hằng `k...`; chỉ sửa nội dung nằm
giữa dấu ngoặc kép. `kKeyboardJson` là JSON của bàn phím lệnh nên cần giữ đúng cú pháp JSON.

Các giá trị thay đổi theo thời gian như IP, firmware, PWM, trạng thái Wi-Fi và RAM được
firmware tự chèn vào các nhãn tương ứng.

## Cấu hình một lần trước khi tự build

1. Tạo bot bằng [@BotFather](https://t.me/BotFather), lấy **Bot Token**.
2. Mở chat với bot và bấm **Start** hoặc gửi `/start`. Nếu dùng nhóm, thêm bot
   vào nhóm rồi gửi `/status@TenBotCuaBan`.
3. Khi robot/bản mẫu chưa chạy bot này, mở API của chính bot để lấy Chat ID:

   ```text
   https://api.telegram.org/bot<TOKEN_CUA_BAN>/getUpdates
   ```

   Lấy số ở `result[].message.chat.id` của chat cần dùng, **không** lấy `update_id`
   hoặc `message_id`. Chat nhóm thường là số âm. Đây là API chính thức
   [getUpdates](https://core.telegram.org/bots/api#getupdates).
4. Sao chép [telegram_config.example.h](l298n/telegram_config.example.h) thành
   `l298n/telegram_config.local.h` **trong cùng thư mục** rồi điền:

   ```cpp
   #pragma once
   #define L298N_TELEGRAM_ENABLED 1
   #define L298N_TELEGRAM_BOT_TOKEN "TOKEN_CUA_BAN"
   #define L298N_TELEGRAM_CHAT_ID "CHAT_ID_CUA_BAN"
   ```

   Không đặt Token vào README, `config.json` hay source dùng chung. File `.local.h`
   đã được Git ignore. Token được nhúng trong firmware sau khi bạn build;
   không chia sẻ file cấu hình riêng hoặc firmware chứa Token của mình.
5. Tự build và flash biến thể `-l298n` như trước. Ví dụ bản ST7789 240x240:

   ```sh
   python main/boards/bread-compact-wifi-lcd/build_hi_esp.py
   ```

   File cấu hình riêng không bị script build thay thế. Không cần sửa `sdkconfig`.
   Cần ESP-IDF 6.0.1 trở lên (ưu tiên 6.1) và certificate bundle của mbedTLS.
   Cấu hình hiện tại đã bật certificate bundle; nếu tự tắt, module ghi log lỗi
   và không khởi động Telegram, không bỏ qua xác minh TLS.

Không có file `.local.h`, hoặc đặt `L298N_TELEGRAM_ENABLED 0`, thì không tạo task
Telegram và không gửi/nhận tin. Không sử dụng Token hoặc mật khẩu trong ZIP mẫu.

## Cách hoạt động

Module dùng [Telegram Bot API](https://core.telegram.org/bots/api): `getMe` xác
nhận bot, `sendMessage` gửi thông báo, `getUpdates` long polling 10 giây nhận
lệnh. Mỗi lần chỉ đọc một update, giới hạn response 8 KiB; update quá lớn được
bỏ qua khi đọc được ID ở phần đầu. Chỉ đọc trường `message.text` trong chat
được phép, không nhận lệnh qua caption, tin đã sửa hoặc chat khác. Trong một
nhóm được cấu hình, mọi thành viên gửi lệnh cho bot đều có thể xem trạng thái,
đánh thức và đặt tốc độ motor.

Task riêng có độ ưu tiên thấp và HTTP timeout 15 giây. Không gọi HTTPS trong
callback Wi-Fi, task audio, main loop hoặc timer motor. Kết nối HTTPS được tái
sử dụng; có thời gian chờ khi lỗi và tôn trọng `retry_after` khi Telegram giới
hạn tần suất. Bot không tạo hàng đợi chuyển động và không đổi chế độ lái.

TLS kiểm tra chứng chỉ máy chủ bằng CA bundle của ESP-IDF. Nếu đồng hồ chưa
được OTA đồng bộ, module khởi động SNTP với `pool.ntp.org` và đợi giờ hợp lệ
trước khi gửi. Khi mạng không cho NTP và OTA cũng không đồng bộ được giờ,
Telegram sẽ chờ, còn giọng nói/web/motor vẫn hoạt động theo luồng hiện tại.

Khi boot, module bỏ qua backlog Telegram trước khi gửi thông báo sẵn sàng.
Hãy gửi lệnh mới sau thông báo đó. Offset chỉ giữ trong RAM; mất mạng trong
cùng lần chạy sẽ tiếp tục từ offset hiện tại. Nếu phản hồi `/status` gửi thất
bại, không xếp hàng gửi lại: gửi lại lệnh khi mạng hồi phục.

Chỉ chạy **một chương trình nhận getUpdates cho mỗi bot**. Không đồng thời
chạy bản mẫu Kylio hoặc một robot khác với cùng Token. `getUpdates` cũng không
hoạt động khi bot đã cấu hình webhook; module không tự xóa webhook của bạn.

## Kiểm tra sau khi bạn build/flash

1. Chưa tạo cấu hình riêng: log `L298nTelegram: Disabled...`, không có task Telegram.
2. Cấu hình đúng: robot kết nối hotspot, bot gửi thông báo và link web. Mở link
   kiểm tra đúng IP/port; cấp quyền bằng nút **Bắt đầu** trên web như trước.
3. Gửi `/status` khi chờ, khi robot nói, khi đang giữ nút web: các trạng thái
   thay đổi tương ứng; `/status` không làm motor dừng/chạy hoặc đổi hướng.
4. Thử `/menu`, `/web`, `/help` và tên bot trong nhóm. Thử từ Chat ID khác:
   không nhận được trả lời và motor không bị tác động.
5. Ngắt/bật lại hotspot: motor web vẫn tự dừng theo cơ chế hiện có; Telegram
   hồi phục khi Internet trở lại. Nếu IP đổi, nhận link mới; không báo boot
   giả khi chỉ nối lại cùng IP.
6. Khởi động lại: nhận một thông báo boot mới, không trả lời hàng loạt tin cũ.
   Kiểm tra âm thanh/mắt/web khi Telegram nhận liên tiếp vài lệnh.
7. Gửi `/speed`, `/speed 50`, `/speed@TenBotCuaBan 75`: tốc độ đã đặt cập nhật
   trên `/status`, motor đang dừng không tự chạy. Thử `0`, `100`, `-1`, `101`,
   `50.5`, `50 abc` và một chuỗi số rất dài; chỉ số nguyên trong 0–100 được nhận.
8. Khi chạy bằng giọng nói, đổi tốc độ: PWM đổi, thời hạn dừng không kéo dài;
   `/speed 0` dừng và không khởi động lại chuyển động cũ khi đặt tốc độ khác.
   Khi web giữ quyền, `/speed 50` và `/speed 0` đều bị từ chối, không đổi PWM web.
   Trả quyền trên web rồi thử lại. Khởi động lại và kiểm tra tốc độ mặc định 75%.

Log lỗi có tag `L298nTelegram` và chỉ ghi tên phương thức, HTTP status, mã lỗi;
không ghi Token hoặc nội dung tin nhắn:

| Log | Kiểm tra |
| --- | --- |
| `Fill BOT_TOKEN and numeric CHAT_ID...` | File cấu hình chưa điền đúng định dạng |
| HTTP 401 | Token không hợp lệ hoặc đã bị thu hồi |
| HTTP 403 | Chưa Start bot, bot bị chặn hoặc không có quyền gửi vào chat |
| HTTP 400 khi `sendMessage` | Chat ID sai hoặc bot chưa được thêm vào nhóm |
| HTTP 409 khi `getUpdates` | Có chương trình polling khác hoặc webhook đang dùng |
| HTTP 429 | Telegram giới hạn tần suất; module tự chờ |
| HTTP 0 / lỗi TLS | Internet, DNS, giờ hệ thống hoặc certificate bundle |

Phần Telegram chưa được build/flash hoặc kiểm thử với bot thật trong phiên
chỉnh sửa này; các bước trên cần kiểm tra trên board của bạn.
