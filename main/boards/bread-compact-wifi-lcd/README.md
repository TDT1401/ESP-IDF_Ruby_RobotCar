# Bread Compact Wi-Fi LCD

**English** | [Tiếng Việt](README.vi.md)

ESP32-S3-N16R8 board with an I2S microphone, MAX98357A amplifier, and SPI TFT display.

## Features

- Voice assistant.
- Follow mode.
- Autonomous obstacle avoidance.
- Manual web control with a HUD.
- Web control at `http://<IP-address>:8080`.
- Real-time obstacle distance measurement.
- Two FC-03 encoders: wheel pulses, RPM and measured forward/backward moves.
- Telegram integration.

## Source layout

Robot features are grouped under `robot/`:

```text
robot/
├── controller/  # Motor control, autonomy, gestures and MCP tools
├── sensors/     # FC-03 encoders and HC-SR04 distance sensor
├── web/         # HTTP server and embedded control page
├── telegram/    # Telegram integration, messages and local configuration
└── tests/       # Browser control regression tests
```

`compact_wifi_board_lcd.cc` initializes these modules. Board pins remain in
`config.h`; `main/CMakeLists.txt` selects the robot sources when
`CONFIG_BREAD_COMPACT_WIFI_LCD_L298N` is enabled.

Run the browser tests from the repository root:

```sh
node --test main/boards/bread-compact-wifi-lcd/robot/tests/web_page_test.cjs
```

## Components

| Quantity | Component                  | Notes             |
| -------- | -------------------------- | ----------------- |
| 1        | Chassis                    |                   |
| 4        | DC motors                  |                   |
| 4        | Wheels                     |                   |
| 1        | L298N motor driver         |                   |
| 1        | Speaker                    |                   |
| 1        | MAX98357A amplifier module | Speaker amplifier |
| 1        | I2S microphone             |                   |
| 1        | ESP32-S3 board             | ESP32-S3-N16R8    |
| 1        | SPI TFT display            |                   |
| 2        | 18650 cells                | 3.7 V per cell    |
| 1        | Power switch               |                   |

## Wiring

Connect the ESP32-S3, all modules, and the motor supply to a common GND. Use the voltage specified in each table.

### I2S microphone

| Microphone pin | ESP32-S3 pin |
| -------------- | ------------ |
| VDD            | 3V3          |
| GND            | GND          |
| WS             | GPIO1        |
| SCK            | GPIO2        |
| SD             | GPIO17       |
| L/R            | GND          |

### MAX98357A amplifier

| MAX98357A pin | Connection    |
| ------------- | ------------- |
| VIN           | 5V            |
| GND           | GND           |
| DIN           | GPIO7         |
| BCLK          | GPIO15        |
| LRC           | GPIO16        |
| `+` / `-`     | Speaker leads |
| SD            | Leave open    |
| GAIN          | Leave open    |

### SPI TFT display

| TFT pin | ESP32-S3 pin |
| ------- | ------------ |
| GND     | GND          |
| VCC     | 3V3          |
| SCL     | GPIO21       |
| SDA     | GPIO47       |
| RES     | GPIO45       |
| DC      | GPIO40       |
| CS      | GPIO41       |
| BLK     | GPIO42       |

### L298N motor driver

| Motor channel | L298N pin  | Connection            | Function              |
| ------------- | ---------- | --------------------- | --------------------- |
| Left          | ENA        | GPIO8                 | Left motor PWM        |
| Left          | IN1        | GPIO9                 | Left motor direction  |
| Left          | IN2        | GPIO10                | Left motor direction  |
| Right         | IN3        | GPIO11                | Right motor direction |
| Right         | IN4        | GPIO12                | Right motor direction |
| Right         | ENB        | GPIO13                | Right motor PWM       |
| Power         | GND        | Common GND            | Motor supply ground   |
| Left          | OUT1, OUT2 | Two left motor wires  | Motor output          |
| Right         | OUT3, OUT4 | Two right motor wires | Motor output          |

- Remove the `ENA`/`ENB` jumpers. Never connect 5 V to an ESP32-S3 GPIO.
- Power the motors from a separate supply through the L298N and connect its GND to common GND.
- A 10 kΩ pull-down resistor from each of `ENA` and `ENB` to GND is recommended.

### HC-SR04 ultrasonic sensor

The single `bread-compact-wifi-lcd` build enables the HC-SR04 and L298N.

| HC-SR04 pin | ESP32-S3 pin | Notes                                           |
| ----------- | ------------ | ----------------------------------------------- |
| VCC         | 5V           | Do not power from 3V3                           |
| GND         | GND          | Must share common GND                           |
| TRIG        | GPIO14       | Signal output from the ESP32-S3                 |
| ECHO        | GPIO39       | Reduce 5 V to 3.3 V before connecting to GPIO39 |

For the `ECHO` pin, a voltage divider can use a 1 kΩ resistor from ECHO to GPIO39 and a 2 kΩ resistor from GPIO39 to GND. Never connect the 5 V `ECHO` signal directly to the ESP32-S3.

### FC-03 wheel encoders

| FC-03 pin | Left wheel | Right wheel |
| --------- | ---------- | ----------- |
| VCC       | 3V3        | 3V3         |
| GND       | Common GND | Common GND  |
| D0 / DO   | GPIO38     | GPIO48      |
| A0 / AO   | Leave open | Leave open  |

D0 must use 3.3 V logic. The onboard LED on GPIO48 is disabled when FC-03 is enabled.
Calibration is in [config.h](config.h): `FC03_LEFT_PULSES_PER_REV` and
`FC03_RIGHT_PULSES_PER_REV` set pulses per wheel revolution;
`FC03_LEFT_WHEEL_DIAMETER_MM` and `FC03_RIGHT_WHEEL_DIAMETER_MM` set the outside
wheel diameters in millimetres. Both wheels are set to **22 pulses/rev** and **60 mm**.

## Build and Flash

Use ESP-IDF 6.1 (minimum 6.0.1). This is the only build command for this board: it selects
the ST7789 240×240 display, L298N, HC-SR04, both FC-03 counters, and **“Hi ESP”** as the
wake word. The firmware name is `bread-compact-wifi-lcd`.

```sh
python scripts/build.py bread-compact-wifi-lcd --language vi-VN --wake-word wn9_hiesp
```

After the build completes, connect the ESP32-S3 to your computer and flash the firmware:

```sh
idf.py -p <SERIAL_PORT> flash monitor
```

## Configuration

- To change the saved Wi-Fi network, hold `BOOT` for about 3 seconds while the device is running, then open `http://192.168.4.1` to configure Wi-Fi again.
- To enable microphone logging, set `MIC_DEBUG` to `1` in `config.h`, then rebuild and flash the firmware.

To drive forward 50 cm, say “move forward 50 cm” or call MCP
`self.motor.forward_distance` with `distance_cm=50`; use `self.motor.backward_distance`
to reverse. The motors stop when both wheels reach the target, or earlier on missing
pulses, encoder failure, or a 10-second timeout. Forward travel also stops if HC-SR04
detects a nearby obstacle. `self.motor.get_state` and `/api/status`
report `distance_move_state`, `distance_move_target_cm`, and `distance_move_progress_cm`.
The tool returns when motion starts; `completed` confirms the measured target was reached.
Wheel slip and calibration error can change the real travel distance.

## Telegram Configuration

1. Create a bot with [@BotFather](https://t.me/BotFather) and get its bot token.
2. Send `/start` to the bot, then get `result[].message.chat.id` from:

   ```text
   https://api.telegram.org/bot<YOUR_TOKEN>/getUpdates
   ```

3. Copy [telegram_config.example.h](robot/telegram/telegram_config.example.h) to `robot/telegram/telegram_config.local.h` and fill in:

   ```cpp
   #pragma once
   #define L298N_TELEGRAM_ENABLED 1
   #define L298N_TELEGRAM_BOT_TOKEN "YOUR_BOT_TOKEN"
   #define L298N_TELEGRAM_CHAT_ID "YOUR_CHAT_ID"
   ```

`telegram_config.local.h` is ignored by Git. Do not share this file or firmware containing your token.
