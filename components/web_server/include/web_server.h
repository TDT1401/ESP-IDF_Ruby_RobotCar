#pragma once

#include <cJSON.h>

#include <string>

#include "motor_controller.h"

namespace ruby_robot {

struct WebNetworkStatus {
    bool connected;
    int rssi;
    std::string ip_address;
};

enum class WebWakeResult { Requested, AlreadyListening, Unavailable };

struct WebServerConfig {
    // All callbacks are required and must remain valid until reboot.
    WebNetworkStatus (*read_network)() = nullptr;
    WebWakeResult (*request_wake)() = nullptr;
    // Append owned JSON fields without taking ownership of the parent object.
    bool (*append_telemetry)(cJSON* parent, const L298nMotorStatus& motor) = nullptr;
};

}  // namespace ruby_robot

// Configure once after motor initialization and before networking starts.
void InitializeWebServer(const ruby_robot::WebServerConfig& config);
// Called from serialized network events. Disconnect immediately revokes motion.
void UpdateWebServerNetwork(bool connected);
