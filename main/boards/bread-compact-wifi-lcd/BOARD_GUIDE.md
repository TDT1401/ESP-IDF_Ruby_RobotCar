# Bread Compact Wi-Fi LCD — Wiring

[Tiếng Việt](../../../docs/bread-compact-wifi-lcd.vi.md)

This pin map is for the ESP32-S3-N16R8 board. Disconnect power before wiring. Connect the ESP32-S3, all modules, and the motor supply to a common GND. Use the supply voltage shown for each module; never put a 5 V signal on an ESP32-S3 GPIO.

## I2S microphone

| Microphone pin | Connect to |
| -------------- | ---------- |
| VDD            | 3V3        |
| GND            | GND        |
| WS             | GPIO1      |
| SCK            | GPIO2      |
| SD             | GPIO17     |
| L/R            | GND        |

## MAX98357A amplifier and speaker

| Amplifier pin | Connect to    |
| ------------- | ------------- |
| VIN           | Regulated 5V  |
| GND           | GND           |
| DIN           | GPIO7         |
| BCLK          | GPIO15        |
| LRC           | GPIO16        |
| + / -         | Speaker leads |
| SD            | Leave open    |
| GAIN          | Leave open    |

Connect the speaker to the amplifier's + and - outputs, not to GND.

## SPI TFT display (ST7789 240×240)

The default build disables the display and does not initialize the SPI panel or backlight. The dual-L298N wiring below assigns GPIO21, GPIO40, GPIO41, GPIO42, GPIO45, and GPIO47 to the right L298N. Disconnect the TFT wires from these pins and keep `CONFIG_BREAD_COMPACT_WIFI_LCD_DISPLAY=n`. GPIO18 is now unused. A replacement display requiring fewer pins will be selected and assigned separately.

| Display pin | Connect to |
| ----------- | ---------- |
| GND         | GND        |
| VCC         | 3V3        |
| SCL         |            |
| SDA         |            |
| RES         |            |
| DC          |            |
| CS          |            |
| BLK         |            |

The display pins labeled SCL and SDA carry SPI clock and data.

## Dual L298N conventional-wheel wiring — bread-compact-wifi-lcd build

The car uses four conventional wheels with differential left/right steering. Front wheels are at the vehicle's front, and rear wheels are at its rear. Each L298N bridge drives one motor independently. Retain the HC-SR04 servo on GPIO6, audio, IMU, both encoders, and the current USB/UART pins.

| Control pin | Left L298N | Right L298N |
| ----------- | ---------- | ----------- |
| ENA — PWM   | GPIO8      | GPIO45      |
| IN1         | GPIO9      | GPIO21      |
| IN2         | GPIO10     | GPIO40      |
| IN3         | GPIO11     | GPIO41      |
| IN4         | GPIO12     | GPIO42      |
| ENB — PWM   | GPIO13     | GPIO47      |

GPIO45, connected to right L298N ENA, is a strapping pin selecting VDD_SPI voltage at reset. Remove the ENA jumper and connect a 10 kΩ resistor from GPIO45/ENA to GND to hold it low during boot; do not pull it up to 5 V. This replaces the previous right ENA assignment on GPIO18.

| Output                    | Left L298N       | Right L298N       |
| ------------------------- | ---------------- | ----------------- |
| OUT1 / OUT2 — bridge A     | Rear left wheel  | Front right wheel |
| OUT3 / OUT4 — bridge B     | Front left wheel | Rear right wheel  |

Remove all four ENA/ENB jumpers across both modules so the ESP32 supplies four independent PWM signals. A 10 kΩ pull-down on each ENA/ENB input is recommended. Connect the ESP32, both modules, and the separate motor supply to a common GND. Do not power the motors from the ESP32-S3 3V3 or 5V rail. Both FC-03 encoders are on the front wheels: left on GPIO38, right on GPIO48. The rear wheels do not have encoders.

The single `bread-compact-wifi-lcd` build keeps its existing firmware/OTA identity and uses four conventional wheels with two L298N modules whenever `CONFIG_BREAD_COMPACT_WIFI_LCD_L298N=y`. The target remains ESP32-S3 with 16 MB flash and `partitions/v2/16m.csv` from the project defaults. Build manually in an ESP-IDF environment with:

```sh
python scripts/build.py bread-compact-wifi-lcd
```

The HUD joystick controls forward/backward motion, curved turns, and rotation in place using `left = y + x` and `right = y - x`, normalized together to the selected speed. Both wheels on each side receive the same logical direction and PWM. Speed PI uses the front encoder on each side; rear wheels follow front PWM without separate feedback. Motor LEDC channels are 1/2/4/5; the servo retains channel 3. Stop, network loss, and session expiry disable all four bridges; direction changes include a stop interval.

Distance and angle commands retain front-encoder and IMU feedback. Odometry uses the left/right model for conventional wheels; joystick steering no longer invalidates pose due to lateral motion. Sensor errors, unknown-direction pulses, and sampling gaps can still require a stationary pose reset.

The existing `CONFIG_BREAD_COMPACT_WIFI_LCD_L298N_INVERT_LEFT/RIGHT` options control front wheel inversion. Rear wheels use the default wiring direction without new build options. Verify each wheel's direction and recalibrate wheel diameter, CPR, and gains for the actual conventional wheels. This update has not been built, tested, or validated on hardware.

## HC-SR04 ultrasonic sensor

| HC-SR04 pin | Connect to                                  |
| ----------- | ------------------------------------------- |
| VCC         | Regulated 5V                                |
| GND         | GND                                         |
| TRIG        | GPIO14                                      |
| ECHO        | GPIO39 through a 5 V-to-3.3 V level divider |

For the ECHO divider, connect 1 kΩ from ECHO to GPIO39 and 2 kΩ from GPIO39 to GND. Never connect the 5 V ECHO signal directly to GPIO39.

## HC-SR04 servo — center hold

| Servo wire                   | Connect to                                               |
| ---------------------------- | -------------------------------------------------------- |
| Signal (orange/yellow/white) | GPIO6                                                    |
| V+ (red)                     | Regulated 5 V supply sized for the servo's stall current |
| GND (brown/black)            | Supply GND and ESP32-S3 GND                              |

## FC-03 wheel sensors

| FC-03 pin | Left sensor | Right sensor |
| --------- | ----------- | ------------ |
| VCC       | 3V3         | 3V3          |
| GND       | GND         | GND          |
| D0 / DO   | GPIO38      | GPIO48       |
| A0 / AO   | Leave open  | Leave open   |

The D0/DO outputs must use 3.3 V logic.

## MPU6050 IMU

| MPU6050 pin | Connect to             |
| ----------- | ---------------------- |
| VCC / VIN   | 3V3                    |
| GND         | GND                    |
| SDA         | GPIO4                  |
| SCL         | GPIO5                  |
| INT         | Leave open             |
| AD0         | GND (I²C address 0x68) |

Keep the I²C pull-ups on SDA and SCL at 3.3 V, never 5 V.
