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

The default build temporarily disables the display. The firmware does not initialize the SPI panel or backlight, and GPIO21, GPIO40, GPIO41, GPIO42, GPIO45, and GPIO47 are left unconfigured by the display code. The wiring below is kept for when the screen is reattached. Run `idf.py reconfigure` before a direct build so an older local `sdkconfig` picks up the new display option. To reattach the screen later, set `CONFIG_BREAD_COMPACT_WIFI_LCD_DISPLAY=y` in `config.json` for the canonical build, or enable the option in `idf.py menuconfig`.

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

## L298N motor driver

| L298N pin   | Connect to                                                                                    |
| ----------- | --------------------------------------------------------------------------------------------- |
| ENA         | GPIO8                                                                                         |
| IN1         | GPIO9                                                                                         |
| IN2         | GPIO10                                                                                        |
| IN3         | GPIO11                                                                                        |
| IN4         | GPIO12                                                                                        |
| ENB         | GPIO13                                                                                        |
| OUT1 / OUT2 | Both left motors, in parallel                                                                 |
| OUT3 / OUT4 | Both right motors, in parallel                                                                |
| VS / +12V   | Positive terminal of a separate motor supply, at a voltage suitable for the motors and driver |
| GND         | Motor supply negative terminal and ESP32-S3 GND                                               |

Remove the ENA and ENB jumpers. A 10 kΩ pull-down from each of ENA and ENB to GND is recommended. Do not power the motors from the ESP32-S3 3V3 or 5V rail.

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
