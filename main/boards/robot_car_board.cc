#include <sdkconfig.h>

#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "board_config.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "system_reset.h"
#include "wifi_board.h"
#include "wifi_manager.h"

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
#include "robot/robot_board.h"
#endif

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <ssid_manager.h>
#include <cstdint>
#include <utility>
#include <vector>

#define TAG "RobotCarBoard"

// Log the I2S samples before they enter AFE/Opus, only for this board.
class MicDiagnosticCodec : public NoAudioCodecSimplex {
public:
    MicDiagnosticCodec(int input_rate, int output_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws,
                       gpio_num_t spk_dout, gpio_num_t mic_sck, gpio_num_t mic_ws,
                       gpio_num_t mic_din)
        : NoAudioCodecSimplex(input_rate, output_rate, spk_bclk, spk_ws, spk_dout, mic_sck, mic_ws,
                              mic_din) {
        ESP_LOGI(TAG, "MIC I2S: %d Hz, BCLK=%d, WS=%d, DIN=%d, left slot, 32-bit, debug=%s",
                 input_rate, mic_sck, mic_ws, mic_din, MIC_DEBUG ? "on" : "off");
    }

protected:
    int Read(int16_t* dest, int samples) override {
        // Read here so the diagnostic can distinguish I2S errors from silent
        // sample data. The board's raw samples reach the full 32-bit range;
        // shifting by 12 (as in the generic codec) clips the observed input.
        read_buffer_.resize(samples);
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(rx_handle_, read_buffer_.data(),
                                         read_buffer_.size() * sizeof(int32_t), &bytes_read, 200);
        if (err != ESP_OK) {
            ++read_errors_;
            last_error_ = err;
        } else {
            int count = bytes_read / sizeof(int32_t);
            for (int i = 0; i < count; ++i) {
                dest[i] = static_cast<int16_t>(read_buffer_[i] >> 16);
#if MIC_DEBUG
                if (raw_samples_ == 0 && i == 0) {
                    raw_min_ = raw_max_ = read_buffer_[i];
                }
                if (read_buffer_[i] < raw_min_)
                    raw_min_ = read_buffer_[i];
                if (read_buffer_[i] > raw_max_)
                    raw_max_ = read_buffer_[i];
                int32_t magnitude = dest[i] < 0 ? -static_cast<int32_t>(dest[i]) : dest[i];
                if (magnitude > peak_)
                    peak_ = magnitude;
                absolute_sum_ += magnitude;
                if (dest[i] != 0)
                    ++nonzero_samples_;
#endif
            }
#if MIC_DEBUG
            raw_samples_ += count;
#endif
        }

        int64_t now = esp_timer_get_time();
        if (last_log_us_ == 0)
            last_log_us_ = now;
        if (now - last_log_us_ >= 2000000) {
#if MIC_DEBUG
            if (raw_samples_ > 0) {
                ESP_LOGI(TAG,
                         "MIC RX: samples=%u, raw_min=%ld, raw_max=%ld, pcm_peak=%ld, "
                         "pcm_mean_abs=%u, nonzero=%u, read_errors=%u, last_error=%s",
                         static_cast<unsigned>(raw_samples_), static_cast<long>(raw_min_),
                         static_cast<long>(raw_max_), static_cast<long>(peak_),
                         static_cast<unsigned>(absolute_sum_ / raw_samples_),
                         static_cast<unsigned>(nonzero_samples_),
                         static_cast<unsigned>(read_errors_), esp_err_to_name(last_error_));
            }
            raw_samples_ = 0;
            nonzero_samples_ = 0;
            absolute_sum_ = 0;
            peak_ = 0;
#endif
            // Keep receive failures visible with diagnostics off, at most once
            // per two seconds so a persistent fault cannot flood the serial log.
            if (read_errors_ > 0) {
                ESP_LOGW(TAG, "MIC RX: read_errors=%u, last_error=%s",
                         static_cast<unsigned>(read_errors_), esp_err_to_name(last_error_));
            }
            read_errors_ = 0;
            last_error_ = ESP_OK;
            last_log_us_ = now;
        }
        return err == ESP_OK ? bytes_read / sizeof(int32_t) : 0;
    }

private:
    int64_t last_log_us_ = 0;
    uint32_t read_errors_ = 0;
    esp_err_t last_error_ = ESP_OK;
#if MIC_DEBUG
    uint32_t raw_samples_ = 0;
    uint32_t nonzero_samples_ = 0;
    uint64_t absolute_sum_ = 0;
    int32_t raw_min_ = 0;
    int32_t raw_max_ = 0;
    int32_t peak_ = 0;
#endif
};

class RobotCarBoard : public WifiBoard {
private:
    Button boot_button_;
    bool wifi_reset_pending_ = false;

    void InitializeButtons() {
        boot_button_.OnLongPress([this]() {
            wifi_reset_pending_ = true;
            ESP_LOGI(TAG, "Release BOOT to clear saved Wi-Fi networks");
        });
        boot_button_.OnPressUp([this]() {
            if (!wifi_reset_pending_) {
                return;
            }
            wifi_reset_pending_ = false;
            Application::GetInstance().Schedule([]() {
                ESP_LOGI(TAG, "Clearing saved Wi-Fi networks and restarting");
                WifiManager::GetInstance().StopStation();
                SsidManager::GetInstance().Clear();
                esp_restart();
            });
        });
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

public:
    RobotCarBoard() : boot_button_(BOOT_BUTTON_GPIO, false, 3000) {
        InitializeButtons();
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
        InitializeRobotBoard();
#endif
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
    void SetPowerSaveLevel(PowerSaveLevel level) override {
        (void)level;
        // The local driving page must respond even while the assistant is idle.
        WifiBoard::SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
    }
#endif

    virtual AudioCodec* GetAudioCodec() override {
        static MicDiagnosticCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
                                              AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK,
                                              AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
    void SetNetworkEventCallback(NetworkEventCallback callback) override {
        WifiBoard::SetNetworkEventCallback(
            [this, callback = std::move(callback)](NetworkEvent event, const std::string& data) {
                if (event == NetworkEvent::Connected) {
                    SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
                    UpdateRobotNetwork(true);
                } else if (event == NetworkEvent::Disconnected ||
                           event == NetworkEvent::WifiConfigModeEnter) {
                    UpdateRobotNetwork(false);
                }
                if (callback)
                    callback(event, data);
            });
    }
#endif

    virtual void StartNetwork() override {
        ESP_LOGI(TAG, "BOOT DIAGNOSTIC: display disabled");
        xTaskCreate(
            [](void*) {
                // The audio service is initialized shortly before networking.  Waiting here
                // lets the amplifier settle, while keeping the greeting independent of Wi-Fi.
                vTaskDelay(pdMS_TO_TICKS(1500));
                Application::GetInstance().Schedule([]() {
                    ESP_LOGI(TAG, "Playing localized startup greeting");
                    Application::GetInstance().PlaySound(Lang::Sounds::OGG_WELCOME);
                });
                vTaskDelete(nullptr);
            },
            "boot_greeting", 3072, nullptr, 2, nullptr);
        WifiBoard::StartNetwork();
    }
};

DECLARE_BOARD(RobotCarBoard);
