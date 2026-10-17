#include "telegram.h"

#include <sdkconfig.h>

#if __has_include("telegram_config.local.h")
#include "telegram_config.local.h"
#endif
#ifndef L298N_TELEGRAM_ENABLED
#define L298N_TELEGRAM_ENABLED 0
#endif

#if L298N_TELEGRAM_ENABLED && defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)

#include <esp_app_desc.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <new>
#include <string>

#include "../controller/l298n_controller.h"
#include "../web/web_server.h"
#include "application.h"
#include "audio_codec.h"
#include "board.h"
#include "display.h"
#include "telegram_text.h"
#include "mcp_server.h"
#include "wifi_manager.h"

namespace {
constexpr const char* kTag = "L298nTelegram";
constexpr size_t kMaxResponse = 8192;
constexpr size_t kMaxOutboundText = 4000;
constexpr UBaseType_t kOutboundQueueDepth = 3;
constexpr double kMaxExactId = 4503599627370495.0;  // Telegram IDs have at most 52 bits.
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
using JsonText = std::unique_ptr<char, decltype(&cJSON_free)>;

bool ReadId(const cJSON* object, const char* key, int64_t& value) {
    const auto* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) ||
        std::abs(item->valuedouble) > kMaxExactId ||
        std::floor(item->valuedouble) != item->valuedouble)
        return false;
    value = static_cast<int64_t>(item->valuedouble);
    return true;
}

std::string LowerAscii(std::string text) {
    for (char& c : text)
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
    return text;
}

bool ParseUnsigned(const std::string& text, int maximum, int& value) {
    if (text.empty() || text.size() > 3 ||
        text.find_first_not_of("0123456789") != std::string::npos) {
        return false;
    }
    value = std::atoi(text.c_str());
    return value <= maximum;
}

const char* StateName(DeviceState state) {
    switch (state) {
        case kDeviceStateStarting:
            return L298nTelegramText::kStateStarting;
        case kDeviceStateWifiConfiguring:
            return L298nTelegramText::kStateWifiConfiguring;
        case kDeviceStateIdle:
            return L298nTelegramText::kStateIdle;
        case kDeviceStateConnecting:
            return L298nTelegramText::kStateConnecting;
        case kDeviceStateListening:
            return L298nTelegramText::kStateListening;
        case kDeviceStateSpeaking:
            return L298nTelegramText::kStateSpeaking;
        case kDeviceStateNotifying:
            return L298nTelegramText::kStateNotifying;
        case kDeviceStateUpgrading:
            return L298nTelegramText::kStateUpgrading;
        case kDeviceStateActivating:
            return L298nTelegramText::kStateActivating;
        case kDeviceStateAudioTesting:
            return L298nTelegramText::kStateAudioTesting;
        case kDeviceStateFatalError:
            return L298nTelegramText::kStateFatalError;
        default:
            return L298nTelegramText::kStateUnknown;
    }
}

const char* StateIcon(DeviceState state) {
    switch (state) {
        case kDeviceStateIdle:
            return L298nTelegramText::kStateIconIdle;
        case kDeviceStateListening:
            return L298nTelegramText::kStateIconListening;
        case kDeviceStateSpeaking:
            return L298nTelegramText::kStateIconSpeaking;
        case kDeviceStateFatalError:
            return L298nTelegramText::kStateIconFatalError;
        case kDeviceStateUpgrading:
            return L298nTelegramText::kStateIconUpgrading;
        default:
            return L298nTelegramText::kStateIconDefault;
    }
}

const char* MotionNameVi(const char* motion) {
    if (strcmp(motion, "forward") == 0)
        return L298nTelegramText::kMotionForward;
    if (strcmp(motion, "backward") == 0)
        return L298nTelegramText::kMotionBackward;
    if (strcmp(motion, "turn_left") == 0)
        return L298nTelegramText::kMotionLeft;
    if (strcmp(motion, "turn_right") == 0)
        return L298nTelegramText::kMotionRight;
    return L298nTelegramText::kMotionStopped;
}

const char* NavigationModeVi(const char* mode) {
    if (strcmp(mode, "follow") == 0)
        return L298nTelegramText::kNavigationFollow;
    if (strcmp(mode, "obstacle_avoidance") == 0)
        return L298nTelegramText::kNavigationAvoid;
    return L298nTelegramText::kNavigationManual;
}

const char* RssiQuality(int rssi) {
    if (rssi >= -55)
        return L298nTelegramText::kRssiExcellent;
    if (rssi >= -67)
        return L298nTelegramText::kRssiGood;
    if (rssi >= -75)
        return L298nTelegramText::kRssiFair;
    return L298nTelegramText::kRssiWeak;
}

std::string WebMessage() {
    auto& wifi = WifiManager::GetInstance();
    if (!wifi.IsConnected())
        return L298nTelegramText::kWebWifiDisconnected;
    if (!IsL298nWebServerRunning())
        return L298nTelegramText::kWebServerUnavailable;
    const std::string url = "http://" + wifi.GetIpAddress() + ":8080";
    return std::string(L298nTelegramText::kWebReadyHeader) +
           L298nTelegramText::kWebLinkPrefix + "<a href=\"" + url + "\"><b>" +
           L298nTelegramText::kWebOpenText + "</b></a>\n" + L298nTelegramText::kWebAddressLabel +
           "<code>" + url + "</code>" + L298nTelegramText::kWebReadyFooter;
}

std::string StatusMessage() {
    auto& wifi = WifiManager::GetInstance();
    const auto motor = GetL298nMotorStatus();
    const auto state = Application::GetInstance().GetDeviceState();
    auto& board = Board::GetInstance();
    auto* display = board.GetDisplay();
    auto* codec = board.GetAudioCodec();
    const auto uptime = esp_timer_get_time() / 1000000;
    const int rssi = wifi.GetRssi();
    const bool motor_ok = motor.hardware_error == ESP_OK;
    const char* control_text = L298nTelegramText::kControlVoice;
    if (motor.web_manual)
        control_text = L298nTelegramText::kControlWeb;
    else if (strcmp(motor.autonomy_mode, "manual") != 0)
        control_text = L298nTelegramText::kControlAutonomy;
    const char* motor_state_text = motor.moving    ? L298nTelegramText::kMotorRunning
                                   : motor.pending ? L298nTelegramText::kMotorPending
                                                   : L298nTelegramText::kMotorIdle;
    const char* emotion_text =
        motor.emotion_enabled ? L298nTelegramText::kEmotionOn : L298nTelegramText::kEmotionOff;
    const char* wifi_text = wifi.IsConnected() ? L298nTelegramText::kWifiConnected
                                               : L298nTelegramText::kWifiDisconnected;
    char uptime_text[64];
    snprintf(uptime_text, sizeof(uptime_text), L298nTelegramText::kUptimeFormat,
             static_cast<long long>(uptime / 3600), static_cast<long long>(uptime / 60 % 60),
             static_cast<long long>(uptime % 60));

    std::string message;
    message.reserve(1536);
    message += L298nTelegramText::kStatusHeader;
    message += "\n";
    message += L298nTelegramText::kDivider;
    message += "\n";
    message += L298nTelegramText::kSystemSection;
    message += "\n";
    message += L298nTelegramText::kStateLabel;
    message += StateIcon(state);
    message += " <b>";
    message += StateName(state);
    message += "</b>\n";
    message += L298nTelegramText::kFirmwareLabel;
    message += "<code>";
    message += esp_app_get_description()->version;
    message += "</code>\n";
    message += L298nTelegramText::kUptimeLabel;
    message += uptime_text;

    message += "\n\n";
    message += L298nTelegramText::kDeviceSection;
    message += "\n";
    message += L298nTelegramText::kDisplayDeviceLabel;
    if (display == nullptr) {
        message += L298nTelegramText::kDeviceNoInfo;
    } else if (!display->IsSetupUICalled() || display->width() <= 0 || display->height() <= 0) {
        message += L298nTelegramText::kDisplayNotReady;
    } else {
        message += L298nTelegramText::kDeviceActive;
        message += " · " + std::to_string(display->width()) + "x" +
                   std::to_string(display->height());
    }
    message += "\n";
    message += L298nTelegramText::kMotorDeviceLabel;
    if (!motor_ok) {
        message += L298nTelegramText::kHardwareErrorPrefix;
        message += "Lỗi: ";
        message += esp_err_to_name(motor.hardware_error);
    } else {
        message += motor.moving ? L298nTelegramText::kDeviceActive
                                : L298nTelegramText::kDeviceReady;
    }
    message += "\n";
    message += L298nTelegramText::kSpeakerDeviceLabel;
    if (codec == nullptr || codec->output_sample_rate() <= 0) {
        message += L298nTelegramText::kDeviceNoInfo;
    } else {
        message += codec->output_enabled() ? L298nTelegramText::kDeviceActive
                                           : L298nTelegramText::kDeviceReady;
        message += " · " + std::to_string(codec->output_sample_rate()) + " Hz";
    }
    message += "\n";
    message += L298nTelegramText::kMicrophoneDeviceLabel;
    if (codec == nullptr || codec->input_sample_rate() <= 0) {
        message += L298nTelegramText::kDeviceNoInfo;
    } else {
        message += codec->input_enabled() ? L298nTelegramText::kDeviceActive
                                          : L298nTelegramText::kDeviceReady;
        message += " · " + std::to_string(codec->input_sample_rate()) + " Hz";
    }
    message += "\n";
    message += L298nTelegramText::kSr04DeviceLabel;
    if (motor.distance_valid) {
        message += L298nTelegramText::kDeviceActive;
        message += " · <b>" + std::to_string(motor.measured_distance_cm) + " cm</b>";
    } else {
        message += L298nTelegramText::kSr04NoEcho;
    }

    message += "\n\n";
    message += L298nTelegramText::kMotorSection;
    message += "\n";
    message += L298nTelegramText::kControlLabel;
    message += "<b>";
    message += control_text;
    message += "</b>\n";
    message += L298nTelegramText::kDirectionLabel;
    message += MotionNameVi(motor.motion);
    message += "\n";
    message += L298nTelegramText::kMotionStateLabel;
    message += motor_state_text;
    message += "\n";
    message += L298nTelegramText::kActivePwmLabel;
    message += "<b>" + std::to_string(motor.active_speed_percent) + "%</b>\n";
    message += L298nTelegramText::kSavedSpeedLabel;
    message += std::to_string(motor.speed_percent) + "%\n";
    if (motor.encoder.enabled) {
        const auto& encoder = motor.encoder;
        message += "FC-03: ";
        if (!encoder.ready) {
            message += std::string("lỗi ") + esp_err_to_name(encoder.error) + "\n";
        } else {
            message += "xung trái/phải <b>" + std::to_string(encoder.left.pulses) + "/" +
                       std::to_string(encoder.right.pulses) + "</b>\n";
            if (encoder.calibrated) {
                char measured[256];
                snprintf(measured, sizeof(measured),
                         "RPM trái/phải: %.1f / %.1f\n"
                         "Quãng đường bánh trái/phải: %.3f / %.3f m\n"
                         "Xe đã đi (ước lượng từ lúc bật): <b>%.3f m</b>\n",
                         encoder.left.rpm, encoder.right.rpm, encoder.left.distance_m,
                         encoder.right.distance_m, encoder.travel_m);
                message += measured;
                if (!encoder.distance_complete)
                    message += "⚠ Quãng đường thiếu xung không xác định được chiều quay.\n";
            } else {
                message += "Chưa hiệu chuẩn số xung/vòng và đường kính bánh trong config.h.\n";
            }
            if (encoder.left.no_pulses || encoder.right.no_pulses) {
                message += "⚠ Có lệnh chạy nhưng không có xung: ";
                if (encoder.left.no_pulses)
                    message += "trái ";
                if (encoder.right.no_pulses)
                    message += "phải";
                message += ". Kiểm tra cảm biến hoặc motor.\n";
            }
        }
    }
    message += L298nTelegramText::kEmotionLabel;
    message += emotion_text;
    if (motor.gesture_active)
        message += L298nTelegramText::kEmotionActiveSuffix;
    message += "\n";
    message += L298nTelegramText::kHardwareLabel;
    if (motor_ok) {
        message += L298nTelegramText::kHardwareOk;
    } else {
        message += L298nTelegramText::kHardwareErrorPrefix;
        message += esp_err_to_name(motor.hardware_error);
    }

    message += "\n\n";
    message += L298nTelegramText::kNavigationSection;
    message += "\n";
    message += L298nTelegramText::kNavigationModeLabel;
    message += "<b>";
    message += NavigationModeVi(motor.autonomy_mode);
    message += "</b>\n";
    message += L298nTelegramText::kFollowDistanceLabel;
    message += std::to_string(motor.follow_distance_cm) + " cm\n";
    message += L298nTelegramText::kObstacleDistanceLabel;
    message += std::to_string(motor.obstacle_distance_cm) + " cm\n";
    message += L298nTelegramText::kBlockedLabel;
    message += motor.autonomy_blocked ? L298nTelegramText::kPathBlocked
                                      : L298nTelegramText::kPathClear;

    message += "\n\n";
    message += L298nTelegramText::kNetworkSection;
    message += "\n";
    message += L298nTelegramText::kWifiLabel;
    message += wifi_text;
    message += "\n";
    message += L298nTelegramText::kSignalLabel;
    message += std::to_string(rssi) + L298nTelegramText::kSignalUnitSeparator + RssiQuality(rssi) +
               "\n";
    message += L298nTelegramText::kIpLabel;
    message += "<code>" + wifi.GetIpAddress() + "</code>\n";
    message += L298nTelegramText::kFreeRamLabel;
    message += std::to_string(esp_get_free_heap_size() / 1024) + L298nTelegramText::kMemoryUnit +
               "\n";

    if (wifi.IsConnected() && IsL298nWebServerRunning()) {
        message += "\n";
        message += L298nTelegramText::kStatusWebLinkPrefix;
        message += "<a href=\"http://" + wifi.GetIpAddress() + ":8080\">" +
                   L298nTelegramText::kWebOpenText + "</a>";
    } else {
        message += "\n" + std::string(L298nTelegramText::kWebUnavailableStatus);
    }
    message += "\n\n" + std::string(L298nTelegramText::kStatusFooter);
    return message;
}

class Telegram {
public:
    static Telegram& Instance() {
        static Telegram instance;
        return instance;
    }

    void Start() {
        if (task_ != nullptr)
            return;
        // Validate without printing the token or the private configuration.
        const std::string token = L298N_TELEGRAM_BOT_TOKEN;
        const auto colon = token.find(':');
        const std::string chat = L298N_TELEGRAM_CHAT_ID;
        const size_t first_digit = !chat.empty() && chat.front() == '-' ? 1 : 0;
        const bool valid_token =
            token.size() >= 20 && token.size() <= 160 && colon != std::string::npos && colon > 0 &&
            colon + 1 < token.size() &&
            token.substr(0, colon).find_first_not_of("0123456789") == std::string::npos &&
            token.substr(colon + 1).find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") ==
                std::string::npos;
        const bool valid_chat =
            chat.size() > first_digit && chat.size() <= 17 &&
            chat.find_first_not_of("0123456789", first_digit) == std::string::npos;
        if (!valid_token || !valid_chat) {
            ESP_LOGE(kTag, "Fill BOT_TOKEN and numeric CHAT_ID in telegram_config.local.h");
            return;
        }
        chat_id_ = strtoll(chat.c_str(), nullptr, 10);
        if (chat_id_ == 0 || std::abs(static_cast<double>(chat_id_)) > kMaxExactId) {
            ESP_LOGE(kTag, "Invalid Telegram chat ID");
            return;
        }
        base_url_ = "https://api.telegram.org/bot" + token + "/";
        outbound_queue_ = xQueueCreate(kOutboundQueueDepth, sizeof(char*));
        if (outbound_queue_ == nullptr) {
            ESP_LOGE(kTag, "Could not allocate Telegram outbound queue");
            return;
        }
        if (xTaskCreate([](void* arg) { static_cast<Telegram*>(arg)->Run(); }, "l298n_telegram",
                        8192, this, 1, &task_) != pdPASS) {
            task_ = nullptr;
            vQueueDelete(outbound_queue_);
            outbound_queue_ = nullptr;
            ESP_LOGE(kTag, "Could not allocate Telegram task");
            return;
        }
        RegisterTools();
    }

    void SetConnected(bool connected) {
        connected_.store(connected);
        if (task_ != nullptr)
            xTaskNotifyGive(task_);
    }

private:
    TaskHandle_t task_ = nullptr;
    QueueHandle_t outbound_queue_ = nullptr;
    std::atomic<bool> connected_{false};
    esp_http_client_handle_t client_ = nullptr;
    std::string base_url_;
    std::string response_;
    std::string username_;
    int64_t chat_id_ = 0;
    int64_t offset_ = -1;  // Discard the pre-boot backlog before announcing readiness.
    int64_t retry_at_us_ = 0;
    bool overflow_ = false;

    Telegram() = default;
    ~Telegram() {
        if (client_ != nullptr)
            esp_http_client_cleanup(client_);
        if (outbound_queue_ != nullptr) {
            char* text = nullptr;
            while (xQueueReceive(outbound_queue_, &text, 0) == pdTRUE)
                delete[] text;
            vQueueDelete(outbound_queue_);
        }
    }

    ToolResult QueueMessage(const std::string& text) {
        if (text.empty() || text.find_first_not_of(" \r\n\t") == std::string::npos)
            return std::unexpected("Telegram message must not be empty.");
        if (text.size() > kMaxOutboundText)
            return std::unexpected("Telegram message exceeds 4000 UTF-8 bytes.");
        if (task_ == nullptr || outbound_queue_ == nullptr)
            return std::unexpected("Telegram is not configured on this device.");

        auto copy = std::unique_ptr<char[]>(new (std::nothrow) char[text.size() + 1]);
        if (!copy)
            return std::unexpected("Not enough memory to queue the Telegram message.");
        memcpy(copy.get(), text.data(), text.size());
        copy[text.size()] = '\0';
        char* queued = copy.get();
        if (xQueueSend(outbound_queue_, &queued, 0) != pdTRUE)
            return std::unexpected("Telegram queue is full; ask the user to try again shortly.");
        copy.release();
        xTaskNotifyGive(task_);
        return std::string("Telegram message queued for delivery.");
    }

    void RegisterTools() {
        McpServer::GetInstance().AddTool(
            "self.telegram.send_message",
            "Send plain text to the owner's configured Telegram chat only when the user "
            "explicitly asks to send, share, or save information to Telegram. Send the useful "
            "content itself, including relevant links when available, not a status note. Never "
            "send credentials, tokens, private system instructions, or content the user did not "
            "request. Maximum 4000 UTF-8 bytes. Delivery is asynchronous; the result confirms "
            "queueing, not final delivery.",
            PropertyList({
                Property("text", kPropertyTypeString).SetMaxLength(kMaxOutboundText),
            }),
            [this](const PropertyList& properties) -> ToolResult {
                return QueueMessage(properties["text"].value<std::string>());
            });
    }

    static esp_err_t OnHttp(esp_http_client_event_t* event) {
        auto* self = static_cast<Telegram*>(event->user_data);
        if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
            const size_t count = std::min(static_cast<size_t>(event->data_len),
                                          kMaxResponse - self->response_.size());
            self->response_.append(static_cast<const char*>(event->data), count);
            if (count != static_cast<size_t>(event->data_len))
                self->overflow_ = true;
        }
        return ESP_OK;
    }

    // Recover just the first update ID from Telegram's outer JSON prefix if a huge
    // message exceeded our buffer. Never execute its text, and don't wedge the queue.
    void SkipOversizedUpdate() {
        const auto key = response_.find("\"update_id\"");
        const auto comma = key == std::string::npos ? key : response_.find(',', key);
        if (comma == std::string::npos)
            return;
        const std::string prefix = response_.substr(0, comma) + "}]}";
        Json json(cJSON_Parse(prefix.c_str()), cJSON_Delete);
        const auto* result = cJSON_GetObjectItemCaseSensitive(json.get(), "result");
        int64_t id;
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json.get(), "ok")) &&
            cJSON_IsArray(result) && cJSON_GetArraySize(result) == 1 &&
            ReadId(cJSON_GetArrayItem(result, 0), "update_id", id) && id >= 0)
            offset_ = std::max(offset_, id + 1);
    }

    Json Request(const char* method, const char* body) {
        if (!connected_.load() || esp_timer_get_time() < retry_at_us_)
            return Json(nullptr, cJSON_Delete);
        response_.clear();
        overflow_ = false;
        const std::string url = base_url_ + method;
        esp_err_t error = esp_http_client_set_url(client_, url.c_str());
        // Clearing post_data below also removes Content-Type in ESP-IDF. Restore
        // it for EVERY request before set_post_field checks the existing header.
        if (error == ESP_OK)
            error = esp_http_client_set_header(client_, "Content-Type", "application/json");
        if (error == ESP_OK)
            error = esp_http_client_set_post_field(client_, body, strlen(body));
        if (error == ESP_OK)
            error = esp_http_client_perform(client_);
        esp_http_client_set_post_field(client_, nullptr, 0);
        // Setup/transport errors may leave the previous response status on a reused client.
        const int status = error == ESP_OK ? esp_http_client_get_status_code(client_) : 0;
        if (overflow_) {
            if (strcmp(method, "getUpdates") == 0 && status == 200)
                SkipOversizedUpdate();
            esp_http_client_close(client_);
            ESP_LOGW(kTag, "%s: response too large; ignored", method);
        }
        Json json(!overflow_ && error == ESP_OK ? cJSON_Parse(response_.c_str()) : nullptr,
                  cJSON_Delete);
        if (error == ESP_OK && status == 200 && json &&
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json.get(), "ok")))
            return json;
        if (error != ESP_OK)
            esp_http_client_close(client_);  // Reconnect after a broken keepalive/TLS connection.

        int64_t retry_seconds = 10;
        int64_t requested_retry;
        if (ReadId(cJSON_GetObjectItemCaseSensitive(json.get(), "parameters"), "retry_after",
                   requested_retry) &&
            requested_retry > 0)
            retry_seconds = std::clamp<int64_t>(requested_retry, 10, 86400);
        if (status == 401 || status == 403 || status == 409)
            retry_seconds = std::max<int64_t>(retry_seconds, 60);
        retry_at_us_ = esp_timer_get_time() + retry_seconds * 1000000;
        // Never log request URLs or response bodies: they can contain credentials/user text.
        ESP_LOGW(kTag, "%s failed: HTTP %d, %s; retry in %lld s", method, status,
                 esp_err_to_name(error), static_cast<long long>(retry_seconds));
        return Json(nullptr, cJSON_Delete);
    }

    bool Send(const std::string& text, bool html = false) {
        Json request(cJSON_CreateObject(), cJSON_Delete);
        Json keyboard(cJSON_Parse(L298nTelegramText::kKeyboardJson), cJSON_Delete);
        if (!request || !keyboard ||
            !cJSON_AddStringToObject(request.get(), "chat_id", L298N_TELEGRAM_CHAT_ID) ||
            !cJSON_AddStringToObject(request.get(), "text", text.c_str()))
            return false;
        if (html && !cJSON_AddStringToObject(request.get(), "parse_mode", "HTML"))
            return false;
        if (!cJSON_AddItemToObject(request.get(), "reply_markup", keyboard.get()))
            return false;
        keyboard.release();
        JsonText body(cJSON_PrintUnformatted(request.get()), cJSON_free);
        return body && Request("sendMessage", body.get()) != nullptr;
    }

    void HandleWake() {
        auto& app = Application::GetInstance();
        switch (app.GetDeviceState()) {
            case kDeviceStateIdle:
                app.Schedule([]() {
                    auto& app = Application::GetInstance();
                    // Recheck on the main task: repeated requests must never toggle listening off.
                    // WakeWordInvoke also rechecks idle before executing the deferred wake-up.
                    if (app.GetDeviceState() == kDeviceStateIdle)
                        app.WakeWordInvoke("Hi ESP");
                });
                Send(L298nTelegramText::kWakeRequested, true);
                break;
            case kDeviceStateListening:
                Send(L298nTelegramText::kWakeAlreadyListening, true);
                break;
            case kDeviceStateConnecting:
                Send(L298nTelegramText::kWakeConnecting, true);
                break;
            case kDeviceStateSpeaking:
                Send(L298nTelegramText::kWakeSpeaking, true);
                break;
            default:
                Send(L298nTelegramText::kWakeUnavailable, true);
                break;
        }
    }

    void HandleSpeed(const std::string& argument) {
        if (argument.empty()) {
            const auto motor = GetL298nMotorStatus();
            Send(std::string(L298nTelegramText::kSpeedCurrentPrefix) + "<b>" +
                     std::to_string(motor.speed_percent) + "%</b>\n" +
                     L298nTelegramText::kActivePwmLabel + "<b>" +
                     std::to_string(motor.active_speed_percent) + "%</b>\n\n" +
                     L298nTelegramText::kSpeedUsage,
                 true);
            return;
        }
        // Accept one integer only; reject signs, decimals, extra arguments and overflow.
        if (argument.size() > 3 || argument.find_first_not_of("0123456789") != std::string::npos) {
            Send(L298nTelegramText::kSpeedUsage, true);
            return;
        }
        const int speed = std::atoi(argument.c_str());
        if (speed > 100) {
            Send(L298nTelegramText::kSpeedUsage, true);
            return;
        }
        const auto error = SetL298nMotorSpeed(speed);
        if (error == ESP_ERR_INVALID_STATE) {
            Send(L298nTelegramText::kSpeedWebBusy, true);
        } else if (error != ESP_OK) {
            Send(L298nTelegramText::kSpeedHardwareError, true);
        } else {
            Send(std::string(L298nTelegramText::kSpeedSetPrefix) + "<b>" + std::to_string(speed) +
                     "%</b>\n" + L298nTelegramText::kSpeedSetFooter,
                 true);
        }
    }

    void HandleAutonomy(const std::string& raw_argument, bool follow) {
        const std::string argument = LowerAscii(raw_argument);
        const char* usage = follow ? L298nTelegramText::kFollowUsage
                                   : L298nTelegramText::kAvoidUsage;
        esp_err_t error = ESP_OK;
        if (argument.empty()) {
            Send(usage, true);
            return;
        }
        if (argument == "off") {
            error = follow ? SetL298nFollowMode(false) : SetL298nObstacleAvoidance(false);
        } else if (argument == "on") {
            error = follow ? SetL298nFollowMode(true) : SetL298nObstacleAvoidance(true);
        } else {
            int distance = 0;
            const int maximum = follow ? 200 : 100;
            const int minimum = 10;
            if (!ParseUnsigned(argument, maximum, distance) || distance < minimum) {
                Send(usage, true);
                return;
            }
            error = follow ? SetL298nFollowDistance(distance)
                           : SetL298nObstacleDistance(distance);
            if (error == ESP_OK) {
                error = follow ? SetL298nFollowMode(true) : SetL298nObstacleAvoidance(true);
            }
        }
        if (error == ESP_OK) {
            Send(StatusMessage(), true);
        } else {
            Send(L298nTelegramText::kAutonomyUnavailable, true);
        }
    }

    void Handle(const cJSON* update) {
        const auto* message = cJSON_GetObjectItemCaseSensitive(update, "message");
        int64_t chat;
        if (!ReadId(cJSON_GetObjectItemCaseSensitive(message, "chat"), "id", chat) ||
            chat != chat_id_)
            return;  // Only the configured private chat/group can send robot commands.
        const auto* text = cJSON_GetObjectItemCaseSensitive(message, "text");
        if (!cJSON_IsString(text) || strlen(text->valuestring) > 128)
            return;
        std::string command = text->valuestring;
        const auto begin = command.find_first_not_of(" \r\n\t");
        if (begin == std::string::npos)
            return;
        command.erase(0, begin);
        const auto end = command.find_first_of(" \r\n\t");
        std::string argument;
        if (end != std::string::npos) {
            const auto argument_begin = command.find_first_not_of(" \r\n\t", end);
            if (argument_begin != std::string::npos)
                argument = command.substr(argument_begin,
                                          command.find_last_not_of(" \r\n\t") - argument_begin + 1);
            command.resize(end);
        }
        command = LowerAscii(command);
        const auto at = command.find('@');
        if (at != std::string::npos) {
            if (command.substr(at + 1) != username_)
                return;
            command.resize(at);
        }
        if (command == "/status")
            Send(StatusMessage(), true);
        else if (command == "/web")
            Send(WebMessage(), true);
        else if (command == "/wake")
            HandleWake();
        else if (command == "/speed")
            HandleSpeed(argument);
        else if (command == "/follow")
            HandleAutonomy(argument, true);
        else if (command == "/avoid")
            HandleAutonomy(argument, false);
        else if (command == "/stop") {
            StopL298nMotor();
            Send(L298nTelegramText::kEmergencyStopped, true);
        } else if (command == "/start" || command == "/menu" || command == "/help")
            Send(L298nTelegramText::kMenu, true);
        else if (!command.empty() && command.front() == '/')
            Send(L298nTelegramText::kUnknownCommand, true);
        // Plain chat messages are ignored; setting speed never starts motor movement.
    }

    bool Poll(bool discard) {
        char body[160];
        snprintf(body, sizeof(body),
                 "{\"offset\":%lld,\"limit\":1,\"timeout\":%d,\"allowed_updates\":[\"message\"]}",
                 static_cast<long long>(offset_), discard ? 0 : 10);
        auto json = Request("getUpdates", body);
        if (!json)
            return false;
        const auto* result = cJSON_GetObjectItemCaseSensitive(json.get(), "result");
        if (!cJSON_IsArray(result))
            return false;
        const auto* update = cJSON_GetArrayItem(result, 0);
        if (update != nullptr) {
            int64_t id;
            if (!ReadId(update, "update_id", id) || id < 0 || id < offset_)
                return false;
            offset_ = id + 1;  // Advance even for unauthorized/unsupported messages.
            if (!discard && connected_.load())
                Handle(update);
        } else if (offset_ < 0) {
            offset_ = 0;
        }
        return true;
    }

    void Run() {
        bool sntp_started = false;
        bool backlog_discarded = false;
        bool boot_sent = false;
        char* pending_message = nullptr;
        std::string announced_ip;
        response_.reserve(kMaxResponse);
        for (;;) {
            // Notifications wake this low-priority task on Wi-Fi changes; no network work
            // runs on the application, Wi-Fi callback, motor timer or audio task.
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
            if (!connected_.load() || esp_timer_get_time() < retry_at_us_)
                continue;
            const auto state = Application::GetInstance().GetDeviceState();
            if (state == kDeviceStateUpgrading || state == kDeviceStateFatalError)
                continue;
            if (time(nullptr) < 1704067200) {  // TLS needs a plausible wall clock (2024+).
                if (!sntp_started) {
                    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
                    const auto error = esp_netif_sntp_init(&config);
                    sntp_started = error == ESP_OK || error == ESP_ERR_INVALID_STATE;
                    ESP_LOGI(kTag, "Waiting for system time before Telegram TLS (%s)",
                             esp_err_to_name(error));
                }
                continue;  // OTA's time sync also satisfies this condition.
            }
            if (client_ == nullptr) {
                esp_http_client_config_t config = {};
                config.url = "https://api.telegram.org";
                config.method = HTTP_METHOD_POST;
                config.crt_bundle_attach = esp_crt_bundle_attach;
                config.timeout_ms = 15000;
                config.disable_auto_redirect = true;
                config.keep_alive_enable = true;
                config.event_handler = OnHttp;
                config.user_data = this;
                client_ = esp_http_client_init(&config);
                if (client_ == nullptr)
                    continue;
            }
            if (username_.empty()) {
                auto me = Request("getMe", "{}");
                const auto* user = cJSON_GetObjectItemCaseSensitive(
                    cJSON_GetObjectItemCaseSensitive(me.get(), "result"), "username");
                if (!cJSON_IsString(user) || strlen(user->valuestring) > 64)
                    continue;
                username_ = LowerAscii(user->valuestring);
            }
            if (!backlog_discarded) {
                if (!Poll(true))
                    continue;
                backlog_discarded = true;
            }
            const auto ip = WifiManager::GetInstance().GetIpAddress();
            if (!boot_sent || ip != announced_ip) {
                const std::string announcement =
                    std::string(boot_sent ? L298nTelegramText::kIpChangedHeader
                                          : L298nTelegramText::kBootHeader) +
                    L298nTelegramText::kBootFirmwareLabel + "<code>" +
                    esp_app_get_description()->version + "</code>\n\n" + WebMessage() +
                    L298nTelegramText::kBootFooter;
                if (!Send(announcement, true))
                    continue;
                boot_sent = true;
                announced_ip = ip;
                ESP_LOGI(kTag, "Telegram notification delivered");
            }
            if (pending_message == nullptr)
                xQueueReceive(outbound_queue_, &pending_message, 0);
            if (pending_message != nullptr) {
                if (!Send(pending_message))
                    continue;
                delete[] pending_message;
                pending_message = nullptr;
                ESP_LOGI(kTag, "Queued Telegram message delivered");
            }
            Poll(false);
        }
    }
};
}  // namespace

void InitializeL298nTelegram() { Telegram::Instance().Start(); }

void UpdateL298nTelegramNetwork(bool connected) { Telegram::Instance().SetConnected(connected); }

#else

#include <esp_log.h>

void InitializeL298nTelegram() {
#if L298N_TELEGRAM_ENABLED
    ESP_LOGE("L298nTelegram", "Telegram needs CONFIG_MBEDTLS_CERTIFICATE_BUNDLE; not started");
#else
    ESP_LOGI("L298nTelegram", "Disabled; configure robot/telegram/telegram_config.local.h to enable");
#endif
}

void UpdateL298nTelegramNetwork(bool connected) { (void)connected; }

#endif
