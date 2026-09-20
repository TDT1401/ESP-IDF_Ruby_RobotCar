# Bread Compact Wi-Fi LCD

The `bread-compact-wifi-lcd-st7735-80x160` variant targets an ESP32-S3-N16R8
module with a generic 0.96-inch ST7735S 80x160 SPI display, an I2S microphone,
and a MAX98357A I2S amplifier.

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

## Build

```sh
python scripts/build.py bread-compact-wifi-lcd --name bread-compact-wifi-lcd-st7735-80x160
```

At startup this variant prints
`BOOT DIAGNOSTIC: ST7735S 80x160 firmware is running`, displays
`ST7735 80x160 BOOT OK`, and plays two popup sounds followed by the success
sound after five seconds. Monitor also prints `BOOT AUDIO TEST` with the
configured volume. These signals confirm that the ST7735 80x160 board profile
is present in the flashed firmware and exercise the I2S speaker path.
