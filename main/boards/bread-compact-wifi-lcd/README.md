# Bread Compact Wi-Fi LCD

**English** | [Tiếng Việt](docs/README.vi.md) | [Project overview](../../../README.md)

Board-specific build and hardware manual for the Ruby RobotCar variant. Use this guide for the parts list, wiring, firmware build, flashing, calibration, and optional Telegram setup. For the project overview and learning path, see the [main README](../../../README.md).

The configured board uses an ESP32-S3-N16R8, I2S microphone, MAX98357A amplifier, and SPI TFT display.

## Capabilities enabled on this board

- Voice assistant, manual web/HUD driving, and Telegram status and stop commands.
- Direct PWM output through L298N for forward, backward, and turns.
- HC-SR04 distance, FC-03 wheel pulses/RPM, and raw MPU6050 gyro telemetry.

Measured distance moves, measured angle turns, heading hold, speed PI, follow mode,
and autonomous obstacle avoidance are disabled while the control architecture is redesigned.
The web session watchdog stops the motors after 600 ms without a valid command.

## Source layout

Robot code is under `robot/`. The controller uses `l298n_motor_driver.cc` for wheel
outputs, `motion_executor.cc` for manual command timing, and `web_control.cc` for
the browser session. Sensor tasks report telemetry but do not control wheel PWM.
Board pins remain in `config.h`; `main/CMakeLists.txt` selects the sources.

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
| 1        | MPU6050 IMU                |                   |
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
wheel diameters in millimetres. Current values are **20.3 pulses/rev** for the left wheel,
**20.8 pulses/rev** for the right wheel, and **60 mm** diameter for both wheels. Recalibrate
these values for your own encoders and wheels.

### MPU6050 IMU

The current simplex I2S configuration leaves GPIO4 and GPIO5 available for the IMU. GPIO18 is not used because it is faulty on this car. Connect the MPU6050 over I²C as follows:

| MPU6050 pin | ESP32-S3 pin | Notes |
| ----------- | ------------ | ----- |
| VCC / VIN   | 3V3          | Use 3.3 V for a safe logic level |
| GND         | Common GND   | Share ground with the ESP32-S3 |
| SDA         | GPIO4        | I²C data |
| SCL         | GPIO5        | I²C clock |
| INT         | Leave open   | The firmware polls the sensor |
| AD0         | GND or leave at its default | Selects address `0x68` |

Keep the I²C pull-ups at 3.3 V; never pull SDA or SCL up to 5 V. Many breakout boards already include pull-ups. GPIO4/5 are dedicated to the MPU6050 in this configuration. If AD0 is tied to 3V3, change `MPU6050_I2C_ADDRESS` in [config.h](config.h) to `0x69`.

Mount the module flat and keep the car still for about two seconds after boot while
the gyro calibrates. The firmware reports raw gyro Z rate through `gyro_z_dps`,
`imu_ready`, and `imu_error`. It does not calculate heading or use IMU data to
correct motor PWM. FC-03 values are telemetry only. A left/right button rotates
the motors in opposite directions at fixed PWM; it does not target an angle.
## Build and Flash

Use ESP-IDF 6.1 (minimum 6.0.1). This is the only build command for this board: it selects
the ST7789 240×240 display, L298N, HC-SR04, both FC-03 counters, MPU6050, and
**“Hi ESP”** as the wake word. The firmware name is `bread-compact-wifi-lcd`.

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

Use the web page at `http://<IP-address>:8080` to drive manually with held
buttons or the joystick. Forward and backward apply the same PWM to both
motors without heading or speed feedback. Release the control or lose the web
session for 600 ms and the motors stop. `self.motor.stop` remains available;
`self.motor.get_state` reports PWM and
raw sensor telemetry. Distance, angle, follow, and avoid commands are unavailable.
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
