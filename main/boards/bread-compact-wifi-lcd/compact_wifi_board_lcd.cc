#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "robo_eyes_display.h"
#include "system_reset.h"
#include "wifi_board.h"
#include "wifi_manager.h"

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
#include "l298n/l298n_controller.h"
#include "l298n/l298n_telegram.h"
#include "l298n/l298n_web_server.h"
#endif

#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_err.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <ssid_manager.h>
#include <cstdint>
#include <utility>
#include <vector>

#if defined(LCD_TYPE_ILI9341_SERIAL) || defined(LCD_TYPE_ST7735_SERIAL)
#include "esp_lcd_ili9341.h"
#endif

#if defined(LCD_TYPE_ST7735_SERIAL)
// Draw before LVGL/audio startup, with the same panel rotation and address window.
// Straight grid lines and a round disk distinguish panel addressing from UI rendering.
static void ShowSt7735AddressTest(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel) {
    constexpr int width = DISPLAY_WIDTH;
    constexpr int height = DISPLAY_HEIGHT;
    std::vector<uint16_t> pixels(width * height, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int dx = x - width / 2;
            const int dy = y - height / 2;
            uint16_t color = 0;
            if (dx * dx + dy * dy <= 20 * 20) {
                color = 0x07FF;  // Cyan RGB565.
            }
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1 || x == width / 4 ||
                x == width * 3 / 4) {
                color = 0xFFFF;
            }
            // esp_lcd expects the high byte first on the SPI wire.
            pixels[y * width + x] = static_cast<uint16_t>((color << 8) | (color >> 8));
        }
    }
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, width, height, pixels.data()));
    // A parameter transaction drains queued color DMA before pixels is released.
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, 0x00, nullptr, 0));
    ESP_LOGI("CompactWifiBoardLCD", "ST7735 address test: border, two vertical lines, cyan circle");
    vTaskDelay(pdMS_TO_TICKS(1500));
}

// ST7735R/S "green tab" initialization used by common 0.96-inch 80x160 panels.
static const uint8_t st7735_cmd_b1[] = {0x01, 0x2C, 0x2D};
static const uint8_t st7735_cmd_b2[] = {0x01, 0x2C, 0x2D};
static const uint8_t st7735_cmd_b3[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
static const uint8_t st7735_cmd_b4[] = {0x07};
static const uint8_t st7735_cmd_c0[] = {0xA2, 0x02, 0x84};
static const uint8_t st7735_cmd_c1[] = {0xC5};
static const uint8_t st7735_cmd_c2[] = {0x0A, 0x00};
static const uint8_t st7735_cmd_c3[] = {0x8A, 0x2A};
static const uint8_t st7735_cmd_c4[] = {0x8D, 0xEE};
static const uint8_t st7735_cmd_c5[] = {0x0E};
static const uint8_t st7735_cmd_e0[] = {0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
                                        0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10};
static const uint8_t st7735_cmd_e1[] = {0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
                                        0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10};
static const uint8_t st7735_cmd_colmod[] = {0x05};
static const uint8_t st7735_cmd_madctl[] = {0xC8};
static const uint8_t st7735_cmd_caset[] = {0x00, 0x00, 0x00, 0x4F};
static const uint8_t st7735_cmd_raset[] = {0x00, 0x00, 0x00, 0x9F};

static const ili9341_lcd_init_cmd_t st7735_init_cmds[] = {
    {0x01, nullptr, 0, 150},
    {0x11, nullptr, 0, 500},
    {0xB1, st7735_cmd_b1, sizeof(st7735_cmd_b1), 0},
    {0xB2, st7735_cmd_b2, sizeof(st7735_cmd_b2), 0},
    {0xB3, st7735_cmd_b3, sizeof(st7735_cmd_b3), 0},
    {0xB4, st7735_cmd_b4, sizeof(st7735_cmd_b4), 0},
    {0xC0, st7735_cmd_c0, sizeof(st7735_cmd_c0), 0},
    {0xC1, st7735_cmd_c1, sizeof(st7735_cmd_c1), 0},
    {0xC2, st7735_cmd_c2, sizeof(st7735_cmd_c2), 0},
    {0xC3, st7735_cmd_c3, sizeof(st7735_cmd_c3), 0},
    {0xC4, st7735_cmd_c4, sizeof(st7735_cmd_c4), 0},
    {0xC5, st7735_cmd_c5, sizeof(st7735_cmd_c5), 0},
    {0x20, nullptr, 0, 0},
    {0x3A, st7735_cmd_colmod, sizeof(st7735_cmd_colmod), 0},
    {0x36, st7735_cmd_madctl, sizeof(st7735_cmd_madctl), 0},
    {0x2A, st7735_cmd_caset, sizeof(st7735_cmd_caset), 0},
    {0x2B, st7735_cmd_raset, sizeof(st7735_cmd_raset), 0},
    {0xE0, st7735_cmd_e0, sizeof(st7735_cmd_e0), 0},
    {0xE1, st7735_cmd_e1, sizeof(st7735_cmd_e1), 0},
    {0x13, nullptr, 0, 10},
    {0x29, nullptr, 0, 100},
};
#endif

#if defined(LCD_TYPE_GC9A01_SERIAL)
#include "esp_lcd_gc9a01.h"
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb1, (uint8_t[]){0x80}, 1, 0},
    {0xb2, (uint8_t[]){0x27}, 1, 0},
    {0xb3, (uint8_t[]){0x13}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x05}, 1, 0},
    {0xac, (uint8_t[]){0xc8}, 1, 0},
    {0xab, (uint8_t[]){0x0f}, 1, 0},
    {0x3a, (uint8_t[]){0x05}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x08}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xea, (uint8_t[]){0x02}, 1, 0},
    {0xe8, (uint8_t[]){0x2A}, 1, 0},
    {0xe9, (uint8_t[]){0x47}, 1, 0},
    {0xe7, (uint8_t[]){0x5f}, 1, 0},
    {0xc6, (uint8_t[]){0x21}, 1, 0},
    {0xc7, (uint8_t[]){0x15}, 1, 0},
    {0xf0,
     (uint8_t[]){0x1D, 0x38, 0x09, 0x4D, 0x92, 0x2F, 0x35, 0x52, 0x1E, 0x0C, 0x04, 0x12, 0x14,
                 0x1f},
     14, 0},
    {0xf1,
     (uint8_t[]){0x16, 0x40, 0x1C, 0x54, 0xA9, 0x2D, 0x2E, 0x56, 0x10, 0x0D, 0x0C, 0x1A, 0x14,
                 0x1E},
     14, 0},
    {0xf4, (uint8_t[]){0x00, 0x00, 0xFF}, 3, 0},
    {0xba, (uint8_t[]){0xFF, 0xFF}, 2, 0},
};
#endif

#define TAG "CompactWifiBoardLCD"

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
        std::vector<int32_t> raw(samples);
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(rx_handle_, raw.data(), raw.size() * sizeof(int32_t),
                                         &bytes_read, 200);
        if (err != ESP_OK) {
            ++read_errors_;
            last_error_ = err;
        } else {
            int count = bytes_read / sizeof(int32_t);
            for (int i = 0; i < count; ++i) {
                dest[i] = static_cast<int16_t>(raw[i] >> 16);
#if MIC_DEBUG
                if (raw_samples_ == 0 && i == 0) {
                    raw_min_ = raw_max_ = raw[i];
                }
                if (raw[i] < raw_min_)
                    raw_min_ = raw[i];
                if (raw[i] > raw_max_)
                    raw_max_ = raw[i];
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

class CompactWifiBoardLCD : public WifiBoard {
private:
    Button boot_button_;
    LcdDisplay* display_;
    bool wifi_reset_pending_ = false;

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_CLOCK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
#if defined(LCD_TYPE_ST7735_SERIAL)
        static ili9341_vendor_config_t st7735_vendor_config = {
            .init_cmds = st7735_init_cmds,
            .init_cmds_size = sizeof(st7735_init_cmds) / sizeof(ili9341_lcd_init_cmd_t),
        };
        panel_config.vendor_config = &st7735_vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_ILI9341_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_GC9A01_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds,
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
        };
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
#if defined(LCD_TYPE_ST7735_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, DISPLAY_PANEL_GAP_X, DISPLAY_PANEL_GAP_Y));
        ESP_LOGI(TAG, "ST7735S 80x160 panel initialized (gap=%d,%d)", DISPLAY_PANEL_GAP_X,
                 DISPLAY_PANEL_GAP_Y);
        if (auto* backlight = GetBacklight()) {
            backlight->SetBrightness(100, false);
        }
        ShowSt7735AddressTest(panel_io, panel);
#elif defined(CONFIG_LCD_ST7789_240X240)
        // Set the physical RAM window before SpiLcdDisplay starts the LVGL task.
        // Keep DISPLAY_OFFSET_X/Y at zero so its constructor does not mutate LVGL
        // display offsets while rendering is already running.
        ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, DISPLAY_PANEL_GAP_X, DISPLAY_PANEL_GAP_Y));
        ESP_LOGI(TAG, "ST7789 240x240 panel initialized (gap=%d,%d)", DISPLAY_PANEL_GAP_X,
                 DISPLAY_PANEL_GAP_Y);
#endif
#ifdef LCD_TYPE_GC9A01_SERIAL
        panel_config.vendor_config = &gc9107_vendor_config;
#endif
#if defined(CONFIG_BREAD_COMPACT_WIFI_LCD_ST7735_80X160) || defined(CONFIG_LCD_ST7789_240X240)
        display_ = new RoboEyesDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                       DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                       DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
#else
        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                     DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
#endif
    }

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

    // 物联网初始化，添加对 AI 可见设备
    void InitializeTools() { static LampController lamp(LAMP_GPIO); }

public:
    CompactWifiBoardLCD() : boot_button_(BOOT_BUTTON_GPIO, false, 3000) {
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeTools();
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
        InitializeL298nController();
        InitializeL298nTelegram();
#if defined(CONFIG_BREAD_COMPACT_WIFI_LCD_ST7735_80X160) || defined(CONFIG_LCD_ST7789_240X240)
        static_cast<RoboEyesDisplay*>(display_)->SetEmotionMotionCallback(UpdateL298nEmotion);
#endif
#endif
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static MicDiagnosticCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
                                              AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK,
                                              AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
                                              AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
    void SetNetworkEventCallback(NetworkEventCallback callback) override {
        WifiBoard::SetNetworkEventCallback(
            [callback = std::move(callback)](NetworkEvent event, const std::string& data) {
                if (event == NetworkEvent::Connected) {
                    UpdateL298nWebNetwork(true);
                    UpdateL298nTelegramNetwork(true);
                } else if (event == NetworkEvent::Disconnected ||
                           event == NetworkEvent::WifiConfigModeEnter) {
                    UpdateL298nWebNetwork(false);
                    UpdateL298nTelegramNetwork(false);
                }
                if (callback)
                    callback(event, data);
            });
    }
#endif

    virtual void StartNetwork() override {
#if defined(CONFIG_BREAD_COMPACT_WIFI_LCD_ST7735_80X160) || defined(CONFIG_LCD_ST7789_240X240)
#if defined(CONFIG_BREAD_COMPACT_WIFI_LCD_ST7735_80X160)
        ESP_LOGI(TAG, "BOOT DIAGNOSTIC: ST7735S 80x160 firmware is running");
#else
        ESP_LOGI(TAG, "BOOT DIAGNOSTIC: %dx%d RoboEyes firmware is running", DISPLAY_WIDTH,
                 DISPLAY_HEIGHT);
#endif
        xTaskCreate(
            [](void*) {
                // The audio service is initialized shortly before networking.  Waiting here
                // lets the amplifier settle, while keeping the greeting independent of Wi-Fi.
                vTaskDelay(pdMS_TO_TICKS(1500));
                Application::GetInstance().Schedule([]() {
                    ESP_LOGI(TAG, "Playing localized startup greeting");
                    static_cast<RoboEyesDisplay*>(Board::GetInstance().GetDisplay())
                        ->ShowHappyFor(3500);
                    Application::GetInstance().PlaySound(Lang::Sounds::OGG_WELCOME);
                });
                vTaskDelete(nullptr);
            },
            "boot_greeting", 3072, nullptr, 2, nullptr);
#endif
        WifiBoard::StartNetwork();
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
};

DECLARE_BOARD(CompactWifiBoardLCD);
