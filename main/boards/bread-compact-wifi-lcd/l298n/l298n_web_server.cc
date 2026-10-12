#include "l298n_web_server.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <cJSON.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include "l298n_controller.h"
#include "l298n_web_page.h"

namespace {
constexpr const char* kTag = "L298nWeb";
httpd_handle_t server = nullptr;
std::atomic<bool> server_ready{false};

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
    size_t received = 0;
    while (received < req->content_len) {
        const int count = httpd_req_recv(req, body + received, req->content_len - received);
        if (count <= 0)
            return ESP_FAIL;  // No retry loop on timeouts; motor watchdog is independent.
        received += count;
    }
    body[received] = '\0';
    if (memchr(body, '\0', received) != nullptr)
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid JSON\"}");
    using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
    Json json(cJSON_ParseWithLengthOpts(body, received + 1, nullptr, true), cJSON_Delete);
    uint32_t session, sequence, speed;
    int joystick_x = 0, joystick_y = 0;
    const auto* direction = cJSON_GetObjectItemCaseSensitive(json.get(), "direction");
    const auto* warning_enabled =
        cJSON_GetObjectItemCaseSensitive(json.get(), "warning_enabled");
    const auto* x = cJSON_GetObjectItemCaseSensitive(json.get(), "x");
    const auto* y = cJSON_GetObjectItemCaseSensitive(json.get(), "y");
    const bool has_joystick = x != nullptr || y != nullptr;
    const bool is_joystick = cJSON_IsString(direction) && strcmp(direction->valuestring, "j") == 0;
    if (!cJSON_IsObject(json.get()) ||
        (has_joystick != is_joystick) ||
        cJSON_GetArraySize(json.get()) !=
            (4 + (warning_enabled == nullptr ? 0 : 1) + (has_joystick ? 2 : 0)) ||
        !ReadInteger(json.get(), "session", 1, UINT32_MAX, &session) ||
        !ReadInteger(json.get(), "sequence", 1, UINT32_MAX, &sequence) ||
        !ReadInteger(json.get(), "speed", 0, 100, &speed) || !cJSON_IsString(direction) ||
        (warning_enabled != nullptr && !cJSON_IsBool(warning_enabled)) ||
        (has_joystick && (!ReadSignedInteger(json.get(), "x", -1000, 1000, &joystick_x) ||
                          !ReadSignedInteger(json.get(), "y", -1000, 1000, &joystick_y))))
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid command\"}");

    L298nWebCommand command;
    if (strcmp(direction->valuestring, "f") == 0)
        command = L298nWebCommand::kForward;
    else if (strcmp(direction->valuestring, "b") == 0)
        command = L298nWebCommand::kBackward;
    else if (strcmp(direction->valuestring, "l") == 0)
        command = L298nWebCommand::kLeft;
    else if (strcmp(direction->valuestring, "r") == 0)
        command = L298nWebCommand::kRight;
    else if (strcmp(direction->valuestring, "j") == 0)
        command = L298nWebCommand::kJoystick;
    else if (strcmp(direction->valuestring, "s") == 0)
        command = L298nWebCommand::kStop;
    else if (strcmp(direction->valuestring, "release") == 0)
        command = L298nWebCommand::kRelease;
    else
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid direction\"}");

    if (CommandL298nWebControl(
            session, sequence, command, static_cast<int>(speed),
            warning_enabled == nullptr || cJSON_IsTrue(warning_enabled), joystick_x,
            joystick_y) != ESP_OK)
        return Reply(req, "409 Conflict", "{\"error\":\"Session expired or motor unavailable\"}");
    return Reply(req, "200 OK", "{\"ok\":true}");
}

esp_err_t Status(httpd_req_t* req) {
    if (req->content_len != 0)
        return Reply(req, "400 Bad Request", "{\"error\":\"Invalid request\"}");
    const auto motor = GetL298nMotorStatus();
    char body[384];
    const int length = snprintf(
        body, sizeof(body),
        "{\"distance_valid\":%s,\"distance_cm\":%d,\"autonomy_mode\":\"%s\","
        "\"follow_distance_cm\":%d,\"obstacle_distance_cm\":%d,"
        "\"follow_target_cm\":%d,\"obstacle_stop_cm\":%d,"
        "\"warning_distance_cm\":%d,\"moving\":%s,\"motion\":\"%s\","
        "\"speed_percent\":%d}",
        motor.distance_valid ? "true" : "false", motor.measured_distance_cm,
        motor.autonomy_mode, motor.follow_distance_cm, motor.obstacle_distance_cm,
        motor.follow_target_effective_cm, motor.obstacle_stop_effective_cm,
        motor.web_warning_effective_cm,
        motor.moving ? "true" : "false", motor.motion, motor.active_speed_percent);
    if (length < 0 || static_cast<size_t>(length) >= sizeof(body))
        return Reply(req, "500 Internal Server Error", "{\"error\":\"Status unavailable\"}");
    return Reply(req, "200 OK", body);
}

esp_err_t Autonomy(httpd_req_t* req) {
    char body[96];
    if (!IsControlRequest(req) || req->content_len == 0 || req->content_len >= sizeof(body)) {
        Reply(req, "400 Bad Request", "{\"error\":\"Invalid request\"}");
        return ESP_FAIL;
    }
    size_t received = 0;
    while (received < req->content_len) {
        const int count = httpd_req_recv(req, body + received, req->content_len - received);
        if (count <= 0)
            return ESP_FAIL;
        received += count;
    }
    body[received] = '\0';
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
        return Reply(req, "409 Conflict", "{\"error\":\"Autonomy unavailable\"}");
    return Reply(req, "200 OK", "{\"ok\":true}");
}

esp_err_t Configure(httpd_req_t* req) {
    char body[96];
    if (!IsControlRequest(req) || req->content_len == 0 || req->content_len >= sizeof(body)) {
        Reply(req, "400 Bad Request", "{\"error\":\"Invalid request\"}");
        return ESP_FAIL;
    }
    size_t received = 0;
    while (received < req->content_len) {
        const int count = httpd_req_recv(req, body + received, req->content_len - received);
        if (count <= 0)
            return ESP_FAIL;
        received += count;
    }
    body[received] = '\0';
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
        return Reply(req, "409 Conflict", "{\"error\":\"Configuration unavailable\"}");
    return Reply(req, "200 OK", "{\"ok\":true}");
}

esp_err_t StartServer() {
    if (server != nullptr)
        return ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8080;  // Wi-Fi provisioning keeps port 80.
    config.ctrl_port = 32769;
    config.max_uri_handlers = 6;
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
    server_ready.store(true);
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

bool IsL298nWebServerRunning() { return server_ready.load(); }
