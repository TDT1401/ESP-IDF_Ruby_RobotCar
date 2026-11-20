# AGENTS.md

## Project

Ruby RobotCar is an ESP-IDF C/C++ voice-assistant robot firmware for one ESP32-S3 board, one build configuration, and no physical display. Its source is under `main/boards/`, with exactly one board factory.

Use ESP-IDF v6.1 when possible. The minimum supported SDK is ESP-IDF v6.0.1. IDF 5.x is not supported.

## Architecture

- `main/application.*`: main event loop, protocol lifecycle, and high-level behavior.
- `main/device_state_machine.*`: legal runtime state transitions.
- `main/boards/common/`: board interfaces and reusable hardware/network helpers.
- `main/boards/robot_car_board.cc`: the sole `RobotCarBoard` factory.
- `main/boards/robot/`: robot integration, input adapters, and serialization.
- `config/`: project-owned hardware settings, motion tuning, and `build_config.json` for the single firmware build.
- `components/motor_driver/`: project-owned L298N GPIO/LEDC driver; hardware configuration is supplied by the board.
- `components/wheel_encoder/`, `components/imu/`, `components/ultrasonic_sensor/`: project-owned sensor drivers configured by the board.
- `components/motor_controller/`: motion control, commands, safety, odometry and read-only snapshots; application integration uses callbacks supplied by the board.
- `components/settings/`: project-owned NVS settings wrapper; preserve existing namespaces and keys.
- `components/audio/`: codecs, audio tasks, engines, wake words, and queues; application supplies the codec and asset reader.
- `main/protocols/`: transport-neutral API plus WebSocket and MQTT/UDP.
- `main/display/` and `main/led/`: shared framework display/LED interfaces; this board uses the inherited `NoDisplay` and no backlight.
- `main/mcp_server.*`: common device-side MCP tools and dispatch.
- `main/Kconfig.projbuild`: board and feature configuration.
- `main/CMakeLists.txt`: source, board, locale, font, and asset selection.
- `scripts/build.py`: canonical single-project build entry point; no board or variant argument.
- `scripts/gen_lang.py` and `scripts/build_default_assets.py`: required language and speech-model/assets generation helpers.
- `partitions/v2/16m.csv`: the sole partition table; preserve its path, offsets and sizes.

Read the closest existing implementation before adding a new one. Prefer the narrowest owning layer; do not put board-specific behavior into core modules.

## Required Rules

- Preserve unrelated worktree changes and keep patches focused.
- A build must export exactly one board factory through `DECLARE_BOARD(...)`.
- Preserve pin assignments and OTA identity during structural changes. Do not introduce another board or build variant unless explicitly requested.
- Core code depends on `Board` interfaces, never a concrete board class or project hardware configuration headers. Robot components receive configuration structs from board initialization.
- Treat camera, backlight, display, LED, battery, and similar capabilities as optional.
- Change runtime state through `Application::SetDeviceState()` and the state machine.
- Callbacks may run outside the main task. Schedule application mutations with `Application::Schedule()` or event bits.
- Do not block the main event loop or audio tasks. Avoid unbounded queues and repeated large allocations in audio paths.
- Keep shared message semantics in `Protocol`; verify both transports when changing its contract.
- Validate network input and preserve `cJSON` ownership. NVS keys are persistent API and require migration when changed.
- Guard target-specific features with Kconfig/component rules. Do not assume every target has PSRAM or S3/P4 resources.
- Do not manually edit generated/vendor output: `build/`, `releases/`, `managed_components/`, `components/` (except project-owned `motor_driver`, `wheel_encoder`, `imu`, `ultrasonic_sensor`, `motor_controller`, `settings` and `audio`), `sdkconfig*`, `main/assets/lang_config.h`, or generated mmap headers.
- Format only touched C/C++ files with the repository `.clang-format`; avoid unrelated mass formatting.

## Boards and Configuration

Project configuration is a coupled chain:

`config/build_config.json` -> `scripts/build.py` -> `main/Kconfig.projbuild` -> `main/CMakeLists.txt` -> `main/boards/robot_car_board.cc` and `config/board_config.h`.

`config/build_config.json` has exactly one build entry for ESP32-S3. Preserve its `type` and `builds[0].name` values (`bread-compact-wifi-lcd`): these are `BOARD_TYPE`/`BOARD_NAME` reported to Xiaozhi/OTA, independent of source paths.

`config/board_config.h` includes the audio, robot hardware and motion tuning headers. The configuration directory is a private include path of `main`; components receive configuration structs and must not include these headers.

Keep the remaining Kconfig keys for saved configuration compatibility; the unused `BREAD_COMPACT_WIFI_LCD_DISPLAY` option was removed. `BOARD_TYPE_BREAD_COMPACT_WIFI_LCD` is an internal default, not a user-facing board selector. Follow `docs/custom-board.md` for this project's integration rules.

## Commands

Source the intended ESP-IDF environment first:

```sh
source /path/to/esp-idf/export.sh
idf.py --version
```

```sh
# Inspect the single build's metadata
python3 scripts/build.py --list-boards

# Build the single project configuration
python3 scripts/build.py

# Format/check touched files
clang-format -i <files>
clang-format --dry-run -Werror <files>
```

The build script changes local `sdkconfig` and build state. It reuses `build/`; no additional board/variant build directories are defined.

## Validation

- Board-only change: build the single firmware and smoke-test changed hardware.
- Core, common-board, audio, protocol, display, dependency, Kconfig, or CMake change: validate the ESP32-S3 build and affected feature/protocol paths.
- Protocol changes: verify WebSocket and MQTT/UDP when shared behavior changes.
- Audio changes: verify capture, playback, wake/VAD, interruption, reconnect, and applicable AEC modes.
- UI/assets changes: verify the `NoDisplay` path and partition size.
- Always report what was tested and what still needs physical hardware. A successful build is not hardware validation.

## Authoritative Documentation

- Overview and SDK policy: `README.md`
- Board integration: `docs/custom-board.md`; wiring: `docs/robot/BOARD_GUIDE.md`, `docs/robot/BOARD_GUIDE.vi.md`
- Audio design: `components/audio/README.md`
- Code style: `docs/code_style.md`
- Protocols: `docs/xiaozhi/websocket.md`, `docs/xiaozhi/mqtt-udp.md`, `docs/xiaozhi/mcp-protocol.md`

Keep detailed or fast-changing information in those files, not here. Add a nested `AGENTS.md` only when a subsystem needs specialized instructions.
