# Ruby RobotCar

**English** | [Tiếng Việt](README.vi.md)

An ESP32 mobile robotics learning project. Ruby RobotCar combines the XiaoZhi voice assistant firmware with a four-wheel car chassis so you can explore how embedded software connects hardware, reads sensors, and responds to control commands.

The source is currently configured for one hardware setup: an ESP32-S3-N16R8, L298N motor driver, HC-SR04 ultrasonic sensor, two FC-03 wheel encoders, and a TFT display. For assembly and setup instructions, see the [bread-compact-wifi-lcd board guide](main/boards/bread-compact-wifi-lcd/README.md).

## Where to start

| If you want to… | Read… |
| --- | --- |
| Assemble the robot, wire it, build and flash the firmware | [bread-compact-wifi-lcd board guide](main/boards/bread-compact-wifi-lcd/README.md) |
| Learn how the firmware selects and configures a board | [Custom board guide](docs/custom-board.md) |
| Understand MCP communication between the assistant and device | [MCP protocol documentation](docs/mcp-protocol.md) |
| Learn about the server communication transports | [WebSocket](docs/websocket.md) · [MQTT + UDP](docs/mqtt-udp.md) |

## Hands-on learning path

1. Review the parts list and wiring diagram in the board guide.
2. Assemble the hardware and check the power supply, common ground, and signal voltage levels before powering it on.
3. Install the ESP-IDF version required by the board guide, then build and flash the firmware.
4. Connect the robot to Wi-Fi and try the control page.
5. Explore `main/boards/bread-compact-wifi-lcd/robot/` to see how the car control features are connected to the firmware.

## What you can learn

- Integrating a motor driver, distance sensor, and wheel encoders with an ESP32.
- Building a motion controller that uses sensor feedback.
- Connecting a web interface and MCP tools to device firmware.
- Keeping board-specific hardware features separate from shared modules.
- Extending voice-assistant firmware and communicating with an AI server.

This is a learning and experimentation project. Travel distance depends on encoder calibration and the driving surface; the HC-SR04 in this setup measures forward only. Start motion tests at low speed on a clear, flat surface.

## Origin and license

The voice-assistant firmware is based on the open-source [XiaoZhi ESP32](https://github.com/78/xiaozhi-esp32) project. This repository is released under the [MIT License](LICENSE). Retain the copyright and license notices when using or redistributing the code.

When reporting an issue, include the board, ESP-IDF version, and relevant logs. Do not post Wi-Fi passwords, Telegram tokens, or personal account details in the repository.
