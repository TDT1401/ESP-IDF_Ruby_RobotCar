#include "telegram.h"

#include <sdkconfig.h>

#ifndef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#error "Telegram requires CONFIG_MBEDTLS_CERTIFICATE_BUNDLE"
#endif

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <new>
#include <string>

#include "telegram_text.h"

namespace {
constexpr const char* kTag = "Telegram";
constexpr size_t kMaxResponse = 8192;
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

class Telegram {
public:
    static Telegram& Instance() {
        static Telegram instance;
        return instance;
    }

    bool Start(const ruby_robot::TelegramConfig& config) {
        if (task_ != nullptr)
            return true;
        if (config.can_send == nullptr) {
            ESP_LOGE(kTag, "Telegram requires an application readiness callback");
            return false;
        }
        // Validate without printing the token or the private configuration.
        const auto& token = config.bot_token;
        const auto colon = token.find(':');
        const auto& chat = config.chat_id;
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
            ESP_LOGE(kTag, "Invalid Telegram bot token or numeric chat ID");
            return false;
        }
        const auto chat_id = strtoll(chat.c_str(), nullptr, 10);
        if (chat_id == 0 || std::abs(static_cast<double>(chat_id)) > kMaxExactId) {
            ESP_LOGE(kTag, "Invalid Telegram chat ID");
            return false;
        }
        base_url_ = "https://api.telegram.org/bot" + token + "/";
        chat_id_ = chat;
        can_send_ = config.can_send;
        outbound_queue_ = xQueueCreate(kOutboundQueueDepth, sizeof(char*));
        if (outbound_queue_ == nullptr) {
            ESP_LOGE(kTag, "Could not allocate Telegram outbound queue");
            return false;
        }
        if (xTaskCreate([](void* arg) { static_cast<Telegram*>(arg)->Run(); }, "telegram",
                        8192, this, 1, &task_) != pdPASS) {
            task_ = nullptr;
            vQueueDelete(outbound_queue_);
            outbound_queue_ = nullptr;
            ESP_LOGE(kTag, "Could not allocate Telegram task");
            return false;
        }
        return true;
    }

    void SetConnected(bool connected) {
        connected_.store(connected);
        if (task_ != nullptr)
            xTaskNotifyGive(task_);
    }

    std::expected<std::string, std::string> QueueMessage(const std::string& text) {
        if (text.empty() || text.find_first_not_of(" \r\n\t") == std::string::npos)
            return std::unexpected("Telegram message must not be empty.");
        if (text.size() > ruby_robot::kTelegramMaxOutboundText)
            return std::unexpected("Telegram message exceeds 4000 UTF-8 bytes.");
        if (task_ == nullptr || outbound_queue_ == nullptr)
            return std::unexpected("Telegram is not running on this device.");

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

private:
    TaskHandle_t task_ = nullptr;
    QueueHandle_t outbound_queue_ = nullptr;
    std::atomic<bool> connected_{false};
    esp_http_client_handle_t client_ = nullptr;
    std::string base_url_;
    std::string chat_id_;
    std::string response_;
    bool (*can_send_)() = nullptr;
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
        Json keyboard(cJSON_Parse(R"({"remove_keyboard":true})"), cJSON_Delete);
        if (!request || !keyboard ||
            !cJSON_AddStringToObject(request.get(), "chat_id", chat_id_.c_str()) ||
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

    void Run() {
        bool sntp_started = false;
        bool boot_sent = false;
        char* pending_message = nullptr;
        response_.reserve(kMaxResponse);
        for (;;) {
            // Notifications wake this low-priority task on Wi-Fi changes; no network work
            // runs on the application, Wi-Fi callback, motor timer or audio task.
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
            if (!connected_.load() || esp_timer_get_time() < retry_at_us_)
                continue;
            if (!can_send_())
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
            if (!boot_sent) {
                if (!Send(TelegramText::kBootMessage, true))
                    continue;
                boot_sent = true;
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
        }
    }
};
}  // namespace

bool InitializeTelegram(const ruby_robot::TelegramConfig& config) {
    return Telegram::Instance().Start(config);
}

void UpdateTelegramNetwork(bool connected) { Telegram::Instance().SetConnected(connected); }

std::expected<std::string, std::string> QueueTelegramMessage(const std::string& text) {
    return Telegram::Instance().QueueMessage(text);
}
