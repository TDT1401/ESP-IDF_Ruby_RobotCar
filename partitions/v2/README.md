# RobotCar 16 MB Partition Table

This project keeps one partition table: [16m.csv](16m.csv), for the ESP32-S3-N16R8.
The selected path remains `partitions/v2/16m.csv`, with the partition table at
`0x8000`. The CSV contents, offsets and sizes are unchanged by source cleanup.

## Flash layout

| Partition | Type / subtype | Offset | Size | Purpose |
| --- | --- | --- | --- | --- |
| `nvs` | data / nvs | `0x9000` | `0x4000` (16 KiB) | Persistent device settings |
| `otadata` | data / ota | `0xD000` | `0x2000` (8 KiB) | OTA boot-slot selection |
| `phy_init` | data / phy | `0xF000` | `0x1000` (4 KiB) | PHY initialization data |
| `ota_0` | app / ota_0 | `0x20000` | `0x3F0000` (4032 KiB) | Application slot 0 |
| `ota_1` | app / ota_1 | `0x410000` (automatic) | `0x3F0000` (4032 KiB) | Application slot 1 |
| `assets` | data / spiffs | `0x800000` | 8 MiB | Speech models and shared framework assets |

The `ota_1` offset is left blank in the CSV; ESP-IDF calculates it from the
preceding partition and alignment. The assets partition ends at `0x1000000`,
the end of the 16 MiB flash.

## Assets and Xiaozhi communication

The firmware uses the `assets` partition for ESP-SR speech models as well as
shared framework resources. Removing the physical screen does not remove these
dependencies.

The build pipeline still uses:

- `scripts/build.py` for the single project build.
- `scripts/gen_lang.py` for localized strings and embedded sound references.
- `scripts/build_default_assets.py` for packing the selected speech models and
  framework resources into the assets image.

The old v1 tables and alternative flash-size/chip tables were removed because
this project uses only the 16 MB ESP32-S3 configuration. The active partition
layout, SDK configuration, OTA identity and Xiaozhi transport code remain unchanged.
