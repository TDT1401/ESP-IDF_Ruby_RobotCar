# RobotCar Board Integration Guide

This repository builds one Ruby RobotCar firmware for ESP32-S3-N16R8. Board source
paths are independent of the identity reported to Xiaozhi/OTA.

## Source layout

```text
main/boards/
├── common/                   # Board interfaces and shared hardware/network helpers
├── robot_car_board.cc        # RobotCarBoard, the single DECLARE_BOARD factory
└── robot/                    # Initialization, web, MCP, serialization

components/telegram/          # Send-only HTTPS task, outbound queue and retry

config/
├── build_config.json         # Single build and existing OTA identity
├── board_config.h            # Aggregated board configuration
├── audio_config.h
├── robot_hardware_config.h
├── motion_control_config.h
├── telegram_config.example.h
└── telegram_config.local.h    # Required private credentials, ignored by Git
```

CMake explicitly registers `main/boards/robot_car_board.cc`. There is no board
directory selector or automatic board discovery. Keep exactly one
`DECLARE_BOARD(RobotCarBoard)`.

## Build configuration and identity

`config/build_config.json` contains one `builds` entry and target `esp32s3`.
Both top-level `type` and `builds[0].name` remain `bread-compact-wifi-lcd`.
CMake reads these as `BOARD_TYPE` and `BOARD_NAME`; the firmware reports them to
Xiaozhi/OTA. Do not derive either identity from source directory names.

The remaining `CONFIG_BREAD_COMPACT_WIFI_LCD_*` keys are retained for saved
configuration compatibility. The unused display option has been removed.
L298N motors with HC-SR04 and both FC-03 encoders are always enabled for this
board through hidden Kconfig defaults; they are not user-selectable features.
`BOARD_TYPE_BREAD_COMPACT_WIFI_LCD` is hidden in Kconfig and defaults on for
ESP32-S3; it is not a board selector.

Keep ESP32-S3, 16 MB flash, `partitions/v2/16m.csv`, existing pins, and OTA identity
during structural refactoring. See [English wiring](robot/BOARD_GUIDE.md) or
[Hướng dẫn nối dây](robot/BOARD_GUIDE.vi.md) for hardware details.

## Hardware configuration

Only board integration code includes `config/board_config.h`. Its include
directory is private to `main`. Board initialization translates the macros into
configuration structs for `motor_driver`, `wheel_encoder`, `imu`,
`ultrasonic_sensor` and `motor_controller`.

Reusable components must not depend on board source, project configuration
headers, `Application`, or generated assets. The board supplies callbacks for
application state, main-task scheduling, and sounds.

NVS settings and audio live in `components/settings/` and `components/audio/`.
`Application` passes the board's codec and an asset-reader callback to
`AudioService::Initialize()`. Audio does not depend on `main`; its public headers
are in `components/audio/include/`. See the [audio design](../components/audio/README.md).

Motors and encoders keep their existing internal Kconfig guards. This firmware has
no display: `RobotCarBoard` inherits `Board::GetDisplay()` (`NoDisplay`) and the
null backlight. `InitializeRobotBoard()` takes no display parameter. Shared display
framework code remains available to the application.

Preserve LEDC allocations: motors timer 1 and channels 1/2/4/5, servo timer/channel
2/3. Removing the screen does not reassign any motor or sensor pin.

Telegram is always included and initialized in the single firmware build. Its
required private settings are in
`config/telegram_config.local.h`, ignored by Git. Copy the example header in that
directory and fill in the bot token and chat ID before building; never put
credentials in tracked files. The TLS certificate bundle
must be enabled; missing settings or a disabled bundle cause a build error.

`components/telegram/` owns the HTTPS task, boot notification, bounded outbound
queue and retry. It does not poll or handle incoming Telegram commands. The board
passes copied credentials and a callback that pauses sending during OTA or a fatal
error to `InitializeTelegram()`, and forwards connectivity to
`UpdateTelegramNetwork()`. MCP registration remains in
`main/boards/robot/mcp/mcp_tools.cc`; the tool calls `QueueTelegramMessage()`.
The component has no dependency on `main` or project configuration headers.

## Build and flash

Use ESP-IDF v6.1 when possible (minimum v6.0.1). In an initialized ESP-IDF
environment, run these commands manually from the project root:

```sh
python scripts/build.py
idf.py -p <SERIAL_PORT> flash monitor
```

Optional language and wake-word flags still apply to the same build:

```sh
python scripts/build.py --language vi-VN --wake-word wn9_hiesp
```

`--list-boards --json` reports the sole build's metadata. `--list-languages`,
`--list-wake-words`, `--build-options-json` and `--zip` remain available.
No board argument, `--name`, or alternate configuration file is required.

The script continues using `build/` and the existing SDK configuration pipeline.
A source move does not require a new board identity or another build directory.
The required build helpers are `scripts/build.py`, `scripts/gen_lang.py` and
`scripts/build_default_assets.py`. The assets packer includes its own model and
asset packing code; the removed legacy converter and SPIFFS tools are not imported.
Only `partitions/v2/16m.csv` remains; see the [partition layout](../partitions/v2/README.md).
A successful compile does not validate hardware: verify motor direction, stop,
disconnect/timeout behavior, encoder feedback, IMU heading and ultrasonic readings
on the actual robot after relevant changes.
