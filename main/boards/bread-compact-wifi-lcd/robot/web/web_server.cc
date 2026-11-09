#include "web_server.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <cJSON.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include "../controller/l298n_controller.h"
#include "../serialization/fc03_json.h"
#include "../serialization/odometry_json.h"
#include "application.h"
#include "web_page.h"
#include "wifi_manager.h"

namespace {
constexpr const char* kTag = "L298nWeb";
httpd_handle_t server = nullptr;

esp_err_t Reply(httpd_req_t* req, const char* status, const char* body) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, body);
}

// Custom header requires a CORS preflight from other origins; no CORS is enabled here.
// This is a local-network controller, not user authentication.
bool IsControlRequest(httpd_req_t* req) {
    char header[2] = {};
    return httpd_req_get_hdr_value_str(req, "X-Robot-Control", header, sizeof(header)) == ESP_OK &&
           strcmp(header, "1") == 0;
}

size_t ReceiveBody(httpd_req_t* req, char* body) {
    size_t received = 0;
    while (received < req->content_len) {
        const int count = httpd_req_recv(req, body + received, req->content_len - received);
        if (count <= 0)
            return 0;  // No retry loop on timeouts; motor watchdog is independent.
        received += count;
    }
    body[received] = '\0';
    return received;
}

esp_err_t Root(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
    return httpd_resp_send(req, kL298nWebPage, sizeof(kL298nWebPage) - 1);
}

esp_err_t Claim(httpd_req_t* req) {
    if (!IsControlRequest(req) || req->content_len != 0) {
        Reply(req, "400 Bad Request", "{\"error\":\"Invalid request\"}");
        return ESP_FAIL;  // Close rather than leave an unread body on a keepalive connection.
    }
    uint32_t session = 0;
    if (BeginL298nWebControl(&session) != ESP_OK)
        return Reply(req, "409 Conflict", "{\"error\":\"Controller busy or unavailable\"}");
    char body[48];
    snprintf(body, sizeof(body), "{\"session\":%lu}", static_cast<unsigned long>(session));
    return Reply(req, "200 OK", body);
}

bool ReadInteger(const cJSON* json, const char* key, uint32_t minimum, uint32_t maximum,
                 uint32_t* result) {
    const auto* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) || item->valuedouble < minimum ||
        item->valuedouble > maximum || std::floor(item->valuedouble) != item->valuedouble)
        return false;
    *result = static_cast<uint32_t>(item->valuedouble);
    return true;
}

bool ReadSignedInteger(const cJSON* json, const char* key, int minimum, int maximum, int* result) {
    const auto* item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) || item->valuedouble < minimum ||
        item->valuedouble > maximum || std::floor(item->valuedouble) != item->valuedouble)
        return false;
    *result = static_cast<int>(item->valuedouble);
    return true;
}

esp_err_t Command(httpd_req_t* req) {
    char body[192];
    if (!IsControlRequest(req) || req->content_len == 0 || req->content_len >= sizeof(body)) {
        Reply(req, "400 Bad Request", "{\"error\":\"Invalid request\"}");
        return ESP_FAIL;
    }
    const size_t received = ReceiveBody(req, body);
    if (received == 0)
        return ESP_FAIL;
    if (memchr(body, '\0', received) != nullptr)
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid JSON\"}");
    using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
    Json json(cJSON_ParseWithLengthOpts(body, received + 1, nullptr, true), cJSON_Delete);
    uint32_t session, sequence, speed;
    int joystick_x = 0, joystick_y = 0;
    const auto* direction = cJSON_GetObjectItemCaseSensitive(json.get(), "direction");
    const auto* warning_enabled = cJSON_GetObjectItemCaseSensitive(json.get(), "warning_enabled");
    const auto* x = cJSON_GetObjectItemCaseSensitive(json.get(), "x");
    const auto* y = cJSON_GetObjectItemCaseSensitive(json.get(), "y");
    const bool has_joystick = x != nullptr || y != nullptr;
    const bool is_joystick = cJSON_IsString(direction) && strcmp(direction->valuestring, "j") == 0;
    if (!cJSON_IsObject(json.get()) || (has_joystick != is_joystick) ||
        cJSON_GetArraySize(json.get()) !=
            (4 + (warning_enabled == nullptr ? 0 : 1) + (has_joystick ? 2 : 0)) ||
        !ReadInteger(json.get(), "session", 1, UINT32_MAX, &session) ||
        !ReadInteger(json.get(), "sequence", 1, UINT32_MAX, &sequence) ||
        !ReadInteger(json.get(), "speed", 0, 100, &speed) || !cJSON_IsString(direction) ||
        (warning_enabled != nullptr && !cJSON_IsBool(warning_enabled)) ||
        (has_joystick && (!ReadSignedInteger(json.get(), "x", -1000, 1000, &joystick_x) ||
                          !ReadSignedInteger(json.get(), "y", -1000, 1000, &joystick_y))))
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid command\"}");

    const L298nWebDriveRequest drive{session,
                                    sequence,
                                    static_cast<int>(speed),
                                    warning_enabled == nullptr || cJSON_IsTrue(warning_enabled),
                                    is_joystick,
                                    joystick_x,
                                    joystick_y};
    esp_err_t error;
    if (strcmp(direction->valuestring, "release") == 0) {
        error = ReleaseL298nWebControl(drive);
    } else {
        const char* drive_direction = direction->valuestring;
        if (is_joystick)
            drive_direction = joystick_y > 0   ? "f"
                              : joystick_y < 0 ? "b"
                              : joystick_x < 0 ? "l"
                              : joystick_x > 0 ? "r"
                                               : "s";
        if (strcmp(drive_direction, "f") == 0)
            error = MoveL298nForward(L298nLinearRequest{drive});
        else if (strcmp(drive_direction, "b") == 0)
            error = MoveL298nBackward(L298nLinearRequest{drive});
        else if (strcmp(drive_direction, "l") == 0)
            error = TurnL298nLeft(L298nTurnRequest{drive});
        else if (strcmp(drive_direction, "r") == 0)
            error = TurnL298nRight(L298nTurnRequest{drive});
        else if (strcmp(drive_direction, "s") == 0)
            error = StopL298nMotor(&drive);
        else
            return Reply(req, "400 Bad Request", "{\"error\":\"Invalid direction\"}");
    }
    if (error != ESP_OK)
        return Reply(req, "409 Conflict", "{\"error\":\"Session expired or motor unavailable\"}");
    return Reply(req, "200 OK", "{\"ok\":true}");
}

esp_err_t Action(httpd_req_t* req) {
    char body[160];
    if (!IsControlRequest(req) || req->content_len == 0 || req->content_len >= sizeof(body)) {
        Reply(req, "400 Bad Request", "{\"error\":\"Invalid request\"}");
        return ESP_FAIL;
    }
    const size_t received = ReceiveBody(req, body);
    if (received == 0)
        return ESP_FAIL;
    if (memchr(body, '\0', received) != nullptr)
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid JSON\"}");
    using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
    Json json(cJSON_ParseWithLengthOpts(body, received + 1, nullptr, true), cJSON_Delete);
    const auto* action = cJSON_GetObjectItemCaseSensitive(json.get(), "action");
    if (!cJSON_IsObject(json.get()) || !cJSON_IsString(action))
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid action\"}");

    if (strcmp(action->valuestring, "stop") == 0 && cJSON_GetArraySize(json.get()) == 1) {
        StopL298nMotor();
        return Reply(req, "200 OK", "{\"ok\":true}");
    }
    if (strcmp(action->valuestring, "wake") == 0 && cJSON_GetArraySize(json.get()) == 1) {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateListening)
            return Reply(req, "200 OK", "{\"ok\":true,\"already_listening\":true}");
        if (app.GetDeviceState() != kDeviceStateIdle)
            return Reply(req, "409 Conflict", "{\"error\":\"Ruby unavailable\"}");
        app.Schedule([]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle)
                app.WakeWordInvoke("Hi ESP");
        });
        return Reply(req, "200 OK", "{\"ok\":true}");
    }

    if (strcmp(action->valuestring, "reset_odometry") == 0 && cJSON_GetArraySize(json.get()) == 1) {
        if (ResetL298nOdometry() != ESP_OK)
            return Reply(req, "409 Conflict",
                         "{\"error\":\"Stop, release HUD and wait for sensors\"}");
        return Reply(req, "200 OK", "{\"ok\":true}");
    }

    if (strcmp(action->valuestring, "navigate_to") == 0) {
        int x_cm = 0, y_cm = 0;
        uint32_t speed = 0, timeout = 0;
        if (cJSON_GetArraySize(json.get()) != 5 ||
            !ReadSignedInteger(json.get(), "x_cm", -1000, 1000, &x_cm) ||
            !ReadSignedInteger(json.get(), "y_cm", -1000, 1000, &y_cm) ||
            !ReadInteger(json.get(), "speed_percent", 18, 70, &speed) ||
            !ReadInteger(json.get(), "timeout_seconds", 1, 120, &timeout))
            return Reply(req, "400 Bad Request", "{\"error\":\"Invalid navigation goal\"}");
        const auto error = StartL298nCommand(ruby_robot::NavigateRequest{
            x_cm / 100.0, y_cm / 100.0, static_cast<int>(speed), static_cast<int>(timeout),
            ruby_robot::NavigateOwner::kWeb});
        if (error != ESP_OK)
            return Reply(req, "409 Conflict", "{\"error\":\"Navigation unavailable\"}");
        return Reply(req, "200 OK", "{\"ok\":true}");
    }

    const bool turn_left = strcmp(action->valuestring, "turn_left") == 0;
    const bool turn_right = strcmp(action->valuestring, "turn_right") == 0;
    if (turn_left || turn_right) {
        uint32_t angle_degrees = 0;
        if (cJSON_GetArraySize(json.get()) != 2 ||
            !ReadInteger(json.get(), "angle_degrees", 5, 180, &angle_degrees))
            return Reply(req, "400 Bad Request", "{\"error\":\"Invalid angle\"}");
        const L298nTurnRequest request{ruby_robot::AngleTurnRequest{
            static_cast<int>(angle_degrees), 0, ruby_robot::AngleTurnOwner::kWeb}};
        const auto error = turn_left ? TurnL298nLeft(request) : TurnL298nRight(request);
        if (error != ESP_OK)
            return Reply(req, "409 Conflict", "{\"error\":\"Controller unavailable\"}");
        return Reply(req, "200 OK", "{\"ok\":true}");
    }

    const bool set_speed = strcmp(action->valuestring, "speed") == 0;
    const bool forward = strcmp(action->valuestring, "forward") == 0;
    const bool backward = strcmp(action->valuestring, "backward") == 0;
    uint32_t value = 0;
    if ((!set_speed && !forward && !backward) || cJSON_GetArraySize(json.get()) != 2 ||
        !ReadInteger(json.get(), set_speed ? "speed" : "distance_cm", set_speed ? 0 : 5,
                     set_speed ? 100 : 1000, &value))
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid action\"}");
    if (set_speed)
        return Reply(req, "501 Not Implemented", "{\"error\":\"Action is disabled\"}");
    const L298nLinearRequest request{ruby_robot::DistanceDriveRequest{
        static_cast<int>(value), 0, 60, ruby_robot::DistanceDriveOwner::kWeb}};
    const auto error = forward ? MoveL298nForward(request) : MoveL298nBackward(request);
    if (error == ESP_ERR_NOT_SUPPORTED)
        return Reply(req, "501 Not Implemented", "{\"error\":\"Action is disabled\"}");
    if (error != ESP_OK)
        return Reply(req, "409 Conflict", "{\"error\":\"Controller unavailable\"}");
    return Reply(req, "200 OK", "{\"ok\":true}");
}

esp_err_t Status(httpd_req_t* req) {
    if (req->content_len != 0)
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid request\"}");
    const auto motor = GetL298nMotorStatus();
    auto& wifi = WifiManager::GetInstance();
    const auto ip = wifi.GetIpAddress();
    using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
    Json json(cJSON_CreateObject(), cJSON_Delete);
    if (!json || !cJSON_AddBoolToObject(json.get(), "distance_valid", motor.distance_valid) ||
        !cJSON_AddStringToObject(json.get(), "command_mode", motor.command.mode) ||
        !cJSON_AddStringToObject(json.get(), "angle_turn_state", motor.command.angle_turn.state) ||
        !cJSON_AddStringToObject(json.get(), "angle_turn_owner", motor.command.angle_turn.owner) ||
        !cJSON_AddStringToObject(json.get(), "angle_turn_direction",
                                 motor.command.angle_turn.direction) ||
        !cJSON_AddNumberToObject(json.get(), "angle_turn_degrees",
                                 motor.command.angle_turn.angle_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "angle_turn_speed_percent",
                                 motor.command.angle_turn.speed_percent) ||
        !cJSON_AddStringToObject(json.get(), "control_mode",
                                 motor.speed_pi_active ? "speed_pi" : "manual_pwm") ||
        !cJSON_AddNumberToObject(json.get(), "distance_cm", motor.measured_distance_cm) ||
        !cJSON_AddStringToObject(json.get(), "autonomy_mode", motor.autonomy_mode) ||
        !cJSON_AddStringToObject(json.get(), "control_owner", motor.control_owner) ||
        !cJSON_AddBoolToObject(json.get(), "warning_sound_enabled", motor.warning_sound_enabled) ||
        !cJSON_AddNumberToObject(json.get(), "follow_distance_cm", motor.follow_distance_cm) ||
        !cJSON_AddNumberToObject(json.get(), "obstacle_distance_cm", motor.obstacle_distance_cm) ||
        !cJSON_AddNumberToObject(json.get(), "follow_target_cm",
                                 motor.follow_target_effective_cm) ||
        !cJSON_AddNumberToObject(json.get(), "obstacle_stop_cm",
                                 motor.obstacle_stop_effective_cm) ||
        !cJSON_AddNumberToObject(json.get(), "warning_distance_cm",
                                 motor.web_warning_effective_cm) ||
        !cJSON_AddBoolToObject(json.get(), "moving", motor.moving) ||
        !cJSON_AddStringToObject(json.get(), "motion", motor.motion) ||
        !cJSON_AddNumberToObject(json.get(), "speed_percent", motor.active_speed_percent) ||
        !cJSON_AddNumberToObject(json.get(), "saved_speed_percent", motor.speed_percent) ||
        !cJSON_AddNumberToObject(json.get(), "uptime_seconds", esp_timer_get_time() / 1000000) ||
        !cJSON_AddBoolToObject(json.get(), "wifi_connected", wifi.IsConnected()) ||
        !cJSON_AddNumberToObject(json.get(), "wifi_rssi", wifi.GetRssi()) ||
        !cJSON_AddStringToObject(json.get(), "ip_address", ip.c_str()) ||
        !cJSON_AddNumberToObject(json.get(), "free_heap_kb", esp_get_free_heap_size() / 1024) ||
        !cJSON_AddBoolToObject(json.get(), "speed_pi_calibrated", motor.speed_pi_calibrated) ||
        !cJSON_AddBoolToObject(json.get(), "speed_pi_active", motor.speed_pi_active) ||
        !cJSON_AddNumberToObject(json.get(), "speed_pi_target_rpm", motor.speed_pi_target_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "speed_pi_left_target_rpm",
                                 motor.speed_pi_left_target_rpm) ||
        !cJSON_AddNumberToObject(json.get(), "speed_pi_right_target_rpm",
                                 motor.speed_pi_right_target_rpm) ||
        !cJSON_AddStringToObject(json.get(), "speed_pi_fault", motor.speed_pi_fault) ||
        !cJSON_AddNumberToObject(json.get(), "left_pwm_percent", motor.left_pwm_percent) ||
        !cJSON_AddNumberToObject(json.get(), "right_pwm_percent", motor.right_pwm_percent) ||
        !cJSON_AddStringToObject(json.get(), "distance_move_state", motor.distance_move_state) ||
        !cJSON_AddNumberToObject(json.get(), "distance_move_target_cm",
                                 motor.distance_move_target_cm) ||
        !cJSON_AddNumberToObject(json.get(), "distance_move_progress_cm",
                                 motor.distance_move_progress_cm) ||
        !cJSON_AddNumberToObject(json.get(), "turn_target_degrees", motor.turn_target_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "turn_progress_degrees",
                                 motor.turn_progress_degrees) ||
        !cJSON_AddBoolToObject(json.get(), "imu_ready", motor.imu_ready) ||
        !cJSON_AddStringToObject(json.get(), "imu_error", esp_err_to_name(motor.imu_error)) ||
        !cJSON_AddBoolToObject(json.get(), "imu_axis_mismatch", motor.imu_axis_mismatch) ||
        !cJSON_AddBoolToObject(json.get(), "heading_valid", motor.heading_valid) ||
        !cJSON_AddNumberToObject(json.get(), "heading_degrees", motor.heading_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "gyro_z_dps", motor.gyro_z_dps) ||
        !cJSON_AddBoolToObject(json.get(), "heading_hold_active", motor.heading_hold_active) ||
        !cJSON_AddNumberToObject(json.get(), "heading_target_degrees",
                                 motor.heading_target_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "heading_error_degrees",
                                 motor.heading_error_degrees) ||
        !cJSON_AddNumberToObject(json.get(), "heading_correction_rpm",
                                 motor.heading_correction_rpm) ||
        !cJSON_AddStringToObject(json.get(), "heading_fault", motor.heading_fault) ||
        !cJSON_AddBoolToObject(json.get(), "turn_uses_imu", motor.turn_uses_imu) ||
        !AddFc03EncoderJson(json.get(), motor.encoder) ||
        !AddOdometryJson(json.get(), motor.odometry, motor.ekf,
                         motor.command.distance_drive, motor.command.navigate))
        return Reply(req, "500 Internal Server Error", "{\"error\":\"Status unavailable\"}");
    std::unique_ptr<char, decltype(&cJSON_free)> body(cJSON_PrintUnformatted(json.get()),
                                                      cJSON_free);
    if (!body)
        return Reply(req, "500 Internal Server Error", "{\"error\":\"Status unavailable\"}");
    return Reply(req, "200 OK", body.get());
}

esp_err_t Autonomy(httpd_req_t* req) {
    char body[96];
    if (!IsControlRequest(req) || req->content_len == 0 || req->content_len >= sizeof(body)) {
        Reply(req, "400 Bad Request", "{\"error\":\"Invalid request\"}");
        return ESP_FAIL;
    }
    const size_t received = ReceiveBody(req, body);
    if (received == 0)
        return ESP_FAIL;
    if (memchr(body, '\0', received) != nullptr)
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid JSON\"}");

    using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
    Json json(cJSON_ParseWithLengthOpts(body, received + 1, nullptr, true), cJSON_Delete);
    const auto* mode = cJSON_GetObjectItemCaseSensitive(json.get(), "mode");
    const auto* enabled = cJSON_GetObjectItemCaseSensitive(json.get(), "enabled");
    if (!cJSON_IsObject(json.get()) || cJSON_GetArraySize(json.get()) != 2 ||
        !cJSON_IsString(mode) || !cJSON_IsBool(enabled))
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid autonomy command\"}");

    esp_err_t error = ESP_ERR_INVALID_ARG;
    if (strcmp(mode->valuestring, "follow") == 0)
        error = SetL298nFollowMode(cJSON_IsTrue(enabled));
    else if (strcmp(mode->valuestring, "avoid") == 0)
        error = SetL298nObstacleAvoidance(cJSON_IsTrue(enabled));
    else
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid autonomy mode\"}");
    if (error != ESP_OK)
        return Reply(req, "501 Not Implemented", "{\"error\":\"Autonomous modes are disabled\"}");
    return Reply(req, "200 OK", "{\"ok\":true}");
}

esp_err_t Configure(httpd_req_t* req) {
    char body[96];
    if (!IsControlRequest(req) || req->content_len == 0 || req->content_len >= sizeof(body)) {
        Reply(req, "400 Bad Request", "{\"error\":\"Invalid request\"}");
        return ESP_FAIL;
    }
    const size_t received = ReceiveBody(req, body);
    if (received == 0)
        return ESP_FAIL;
    if (memchr(body, '\0', received) != nullptr)
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid JSON\"}");

    using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
    Json json(cJSON_ParseWithLengthOpts(body, received + 1, nullptr, true), cJSON_Delete);
    uint32_t follow_distance, obstacle_distance;
    if (!cJSON_IsObject(json.get()) || cJSON_GetArraySize(json.get()) != 2 ||
        !ReadInteger(json.get(), "follow_distance_cm", 10, 200, &follow_distance) ||
        !ReadInteger(json.get(), "obstacle_distance_cm", 10, 100, &obstacle_distance))
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid configuration\"}");
    if (SetL298nFollowDistance(static_cast<int>(follow_distance)) != ESP_OK ||
        SetL298nObstacleDistance(static_cast<int>(obstacle_distance)) != ESP_OK)
        return Reply(req, "501 Not Implemented",
                     "{\"error\":\"Autonomous settings are disabled\"}");
    return Reply(req, "200 OK", "{\"ok\":true}");
}

esp_err_t StartServer() {
    if (server != nullptr)
        return ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8080;  // Wi-Fi provisioning keeps port 80.
    config.ctrl_port = 32769;
    config.max_uri_handlers = 9;
    config.max_open_sockets = 2;
    config.lru_purge_enable = true;
    config.stack_size = 6144;
    config.recv_wait_timeout = 1;
    config.send_wait_timeout = 1;
    esp_err_t error = httpd_start(&server, &config);
    if (error != ESP_OK) {
        server = nullptr;
        return error;
    }
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = Root, .user_ctx = nullptr},
        {.uri = "/api/claim", .method = HTTP_POST, .handler = Claim, .user_ctx = nullptr},
        {.uri = "/api/command", .method = HTTP_POST, .handler = Command, .user_ctx = nullptr},
        {.uri = "/api/action", .method = HTTP_POST, .handler = Action, .user_ctx = nullptr},
        {.uri = "/api/status", .method = HTTP_GET, .handler = Status, .user_ctx = nullptr},
        {.uri = "/api/config", .method = HTTP_POST, .handler = Configure, .user_ctx = nullptr},
        {.uri = "/api/autonomy", .method = HTTP_POST, .handler = Autonomy, .user_ctx = nullptr},
    };
    for (const auto& route : routes) {
        error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) {
            httpd_stop(server);
            server = nullptr;
            return error;
        }
    }
    return ESP_OK;
}
}  // namespace

void UpdateL298nWebNetwork(bool connected) {
    SetL298nWebConnected(connected);
    if (!connected)
        return;
    const esp_err_t error = StartServer();
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Cannot start web controller: %s", esp_err_to_name(error));
        return;
    }
    auto* station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip = {};
    if (station != nullptr && esp_netif_get_ip_info(station, &ip) == ESP_OK)
        ESP_LOGI(kTag, "Motor control: http://" IPSTR ":8080", IP2STR(&ip.ip));
}
