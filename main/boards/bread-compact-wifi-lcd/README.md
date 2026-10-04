# Bread Compact Wi-Fi LCD

This board targets an ESP32-S3-N16R8 module, an I2S microphone, and a
MAX98357A I2S amplifier. Select the build variant for the connected TFT:

| Variant | TFT |
| --- | --- |
| `bread-compact-wifi-lcd` | ST7789 240x320 |
| `bread-compact-wifi-lcd-st7789-240x240` | 1.54-inch ST7789 240x240 |
| `bread-compact-wifi-lcd-st7735-80x160` | 0.96-inch ST7735S 80x160 |

Optional L298N motor variants append `-l298n` to each name above. See
[L298N wiring, voice commands and validation](L298N.md). Motor control is
disabled in the original variants; the motor variants keep the same audio,
display, flash and partition configuration.

## Wiring

All modules must share ground.

| Module pin | ESP32-S3 pin |
| --- | --- |
| Microphone VDD | 3V3 |
| Microphone GND | GND |
| Microphone WS | GPIO4 |
| Microphone SCK | GPIO5 |
| Microphone SD | GPIO6 |
| Microphone L/R | GND (left channel) |
| MAX98357A VIN | 5V |
| MAX98357A GND | GND |
| MAX98357A DIN | GPIO7 |
| MAX98357A BCLK | GPIO15 |
| MAX98357A LRC | GPIO16 |
| TFT GND | GND |
| TFT VCC | 3V3 |
| TFT SCL | GPIO21 |
| TFT SDA | GPIO47 |
| TFT RES | GPIO45 |
| TFT DC | GPIO40 |
| TFT CS | GPIO41 |
| TFT BLK | GPIO42 |

Leave the MAX98357A `SD` and `GAIN` pins unconnected initially. Connect the
speaker only across the amplifier `+` and `-` outputs; neither speaker terminal
goes to ground.

### Chân kết nối L298N (motor trái/phải)

Áp dụng cho các biến thể firmware có hậu tố `-l298n`. Số GPIO dưới đây là
số tín hiệu trên ESP32-S3, không phải số thứ tự chân trên hàng cắm.

| Kênh | Chân L298N | Chân ESP32-S3 | Chức năng |
| --- | --- | --- | --- |
| Trái | ENA | GPIO8 | PWM điều chỉnh tốc độ motor trái |
| Trái | IN1 | GPIO9 | Điều khiển chiều motor trái |
| Trái | IN2 | GPIO10 | Điều khiển chiều motor trái |
| Phải | IN3 | GPIO11 | Điều khiển chiều motor phải |
| Phải | IN4 | GPIO12 | Điều khiển chiều motor phải |
| Phải | ENB | GPIO13 | PWM điều chỉnh tốc độ motor phải |
| Chung | GND | GND | Nối chung với âm nguồn motor |

- **OUT1/OUT2** nối hai dây motor trái; **OUT3/OUT4** nối hai dây motor phải.
- **Tháo jumper ENA và ENB trước khi nối GPIO** để điều khiển tốc độ bằng PWM.
  Không nhầm với jumper nguồn `5V-EN`; cấu hình jumper nguồn theo module đang dùng.
- Cấp nguồn motor phù hợp vào đầu nguồn công suất L298N, không lấy từ chân
  3V3 ESP32. Không đưa 5V vào GPIO.
- Nên kéo ENA và ENB xuống GND bằng điện trở **10 kΩ cho mỗi chân** để giữ
  motor tắt trong lúc khởi động/reset.
- Nếu một motor quay ngược chiều mong muốn, ngắt nguồn rồi đổi hai dây motor
  đó, hoặc dùng tùy chọn đảo chiều trái/phải trong menuconfig.

Mặc định khi chưa thay đổi tốc độ bằng giọng nói:

| Lệnh | Tốc độ PWM | Thời gian chạy |
| --- | --- | --- |
| Tiến / lùi | 75% | 1 giây |
| Xoay trái / xoay phải | 75% | 0,4 giây (400 ms) |

Chỉ cần nói “xoay phải”, “xoay trái”, “tiến lên” hoặc “lùi lại”; không cần đọc
tốc độ và thời gian. Khi nói rõ thông số, firmware ưu tiên thông số đó.
Tốc độ đã đổi bằng giọng nói được dùng chung cho các lệnh tiếp theo đến khi
đổi lại hoặc khởi động lại; khởi động lại trở về 75%. Thời gian chỉ định chỉ
áp dụng cho lệnh đó; lệnh sau không nêu thời gian sẽ dùng mặc định trong bảng.

Chi tiết lệnh giọng nói và cách kiểm tra: [L298N.md](L298N.md).

## Build

### Câu đánh thức “Hi ESP” cho robot L298N

Sau khi mở môi trường ESP-IDF, tự chạy lệnh sau từ thư mục gốc dự án để build
bản ST7789 240x240 + L298N với câu đánh thức **“Hi ESP”**:

```sh
python main/boards/bread-compact-wifi-lcd/build_hi_esp.py
```

Script chuyển tiếp sang `scripts/build.py` với biến thể
`bread-compact-wifi-lcd-st7789-240x240-l298n` và `--wake-word wn9_hiesp`.
Tham số này chọn mô hình “Hi ESP” thay cho “Ni hao XiaoZhi”. Câu đánh thức
chỉ thay đổi trên robot sau khi bạn build và flash firmware/assets mới;
`idf.py build` với cấu hình cũ không tự chọn mô hình mới.

Để tự gọi lệnh build chuẩn, dùng:

```sh
python scripts/build.py bread-compact-wifi-lcd --name bread-compact-wifi-lcd-st7789-240x240-l298n --wake-word wn9_hiesp
```

### Original display variants (without motors)

```sh
python scripts/build.py bread-compact-wifi-lcd --name bread-compact-wifi-lcd-st7789-240x240
```

The 1.54-inch panel uses the normal 4-wire SPI mode and has no address-window
offset. Use the TFT wiring listed above. This variant keeps the RoboEyes display
and startup welcome sound; it uses partial refresh to avoid allocating a
240x240 DMA frame buffer.

To build the retained 0.96-inch ST7735S alternative:

```sh
python scripts/build.py bread-compact-wifi-lcd --name bread-compact-wifi-lcd-st7735-80x160
```

Both RoboEyes variants log `BOOT DIAGNOSTIC` and play the startup welcome sound.
These signals exercise the I2S speaker path independently of the network
connection. The 240x240 variant should report
`SPI render: 240x240 RGB565, partial refresh, buffer=9600 bytes`.

## Microphone diagnostic

Set `MIC_DEBUG` in this board's `config.h`, then rebuild and flash:

```cpp
#define MIC_DEBUG 0  // Normal use (default).
// Change the value to 1 to enable microphone level logs.
```

With `MIC_DEBUG=0`, the per-sample statistics and periodic level logs are
disabled. The `MIC I2S` startup configuration and rate-limited I2S read error
warnings remain enabled. Both settings preserve the working `>>16` sample
conversion. This flag applies to the board's default simplex I2S codec.

With `MIC_DEBUG=1`, open the serial monitor after flashing. During capture,
the board logs `MIC RX` levels about every two seconds. Speak close to
the microphone, then compare the readings with a quiet interval. To start
capture without a server connection, enter Wi-Fi configuration mode and click
BOOT once to start audio testing; click it again to stop.

- `samples` near 32000 per two seconds and `read_errors=0` mean I2S is returning
  data. `pcm_peak` and `pcm_mean_abs` should rise when you speak.
- Increasing `read_errors` points to I2S receive failures;
  `last_error` gives the ESP-IDF error name.
- `raw_min=raw_max` or `nonzero=0` while speaking means the receive data is
  stuck or silent. Check microphone power and its L/R channel selection against
  the configured left slot, even if the wires are seated correctly.
- Changing raw values with `pcm_peak=0` points to a sample format or bit shift
  mismatch. Changing PCM levels with no recognition points further down the
  audio or network pipeline.

This board converts the 32-bit I2S sample to 16-bit PCM by shifting 16 bits.
Earlier firmware shifted 12 bits and clipped the large raw values seen in mic
logs. A full two-second window of `raw_min=raw_max=0` still indicates that the
I2S data itself is zero; changing the PCM shift cannot repair that condition.

The startup welcome sound tests only speaker output; it does not test the mic.

## Change saved Wi-Fi

After flashing this firmware, hold the BOOT button for 3 seconds while the
device is running, then release it. The device removes only saved Wi-Fi
networks and restarts in configuration mode. Connect another phone or computer
to `Xiaozhi-XXXX`, open `http://192.168.4.1`, and enter the new hotspot name
and password. Do not hold BOOT while powering on or resetting the ESP32-S3;
that would enter its firmware download mode.

## ST7735 display refresh verification

The landscape panel window starts at column 0, so a 160-pixel row ends at
159 (inclusive). A column offset of 1 requests end address 160, which is
outside the range of ST7735 configurations with 160-row memory. The vertical
gap remains 26; do not change rotation or both gaps together when diagnosing
row wrapping.

Before LVGL starts, a 1.5-second diagnostic draws a white rectangular border,
two straight vertical lines and a cyan disk directly through esp_lcd. The
backlight is temporarily enabled without saving a new brightness setting.
If this pattern is skewed, the fault exists below the LVGL UI; if it is clean
but the subsequent eyes are skewed, investigate LVGL rendering/flush instead.
The diagnostic runs during board construction, before audio starts.

The ST7735 RoboEyes display requests a full RGB565 frame buffer (25,600 bytes
at 160x80). On boot, check for:

```text
SPI render: 160x80 RGB565, full refresh, buffer=25600 bytes
```

`partial refresh` means the active display constructor did not request this
mode. A full buffer is a workaround to test redraw corruption, not proof that
panel initialization or wiring is correct. Verify both stationary eye outlines
and repeated blinking on hardware; there should be no leftover pixels.
