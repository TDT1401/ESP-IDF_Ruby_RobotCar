#include "hcsr04_sensor.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "../../config.h"

namespace {

constexpr char kTag[] = "Hcsr04";
constexpr int kSamplePeriodMs = 60;  // HC-SR04 needs the previous acoustic burst to fade.
constexpr int kEchoTimeoutMs = 35;
constexpr int kMinEchoUs = 116;    // Approximately 2 cm.
constexpr int kMaxEchoUs = 23200;  // Approximately 400 cm.

class Hcsr04Sensor {
public:
    static Hcsr04Sensor& Instance() {
        static Hcsr04Sensor sensor;
        return sensor;
    }

    void Start(Hcsr04ReadingCallback callback) {
        if (task_ != nullptr)
            return;
        callback_ = callback;

        gpio_config_t trigger = {};
        trigger.pin_bit_mask = 1ULL << HCSR04_TRIGGER_PIN;
        trigger.mode = GPIO_MODE_OUTPUT;
        trigger.pull_down_en = GPIO_PULLDOWN_ENABLE;
        ESP_ERROR_CHECK(gpio_config(&trigger));
        ESP_ERROR_CHECK(gpio_set_level(HCSR04_TRIGGER_PIN, 0));

        gpio_config_t echo = {};
        echo.pin_bit_mask = 1ULL << HCSR04_ECHO_PIN;
        echo.mode = GPIO_MODE_INPUT;
        echo.pull_down_en = GPIO_PULLDOWN_ENABLE;
        echo.intr_type = GPIO_INTR_ANYEDGE;
        ESP_ERROR_CHECK(gpio_config(&echo));

        const auto isr_error = gpio_install_isr_service(0);
        if (isr_error != ESP_OK && isr_error != ESP_ERR_INVALID_STATE) {
            ESP_ERROR_CHECK(isr_error);
        }
        ESP_ERROR_CHECK(gpio_isr_handler_add(HCSR04_ECHO_PIN, EchoIsr, this));
#ifdef CONFIG_PM_ENABLE
        ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "hcsr04", &sleep_lock_));
#endif

        const auto task_result = xTaskCreate(
            [](void* arg) { static_cast<Hcsr04Sensor*>(arg)->Run(); }, "hcsr04", 3072, this, 2,
            &task_);
        if (task_result != pdPASS) {
            task_ = nullptr;
            ESP_ERROR_CHECK(gpio_isr_handler_remove(HCSR04_ECHO_PIN));
#ifdef CONFIG_PM_ENABLE
            esp_pm_lock_delete(sleep_lock_);
            sleep_lock_ = nullptr;
#endif
            ESP_LOGE(kTag, "Could not allocate sensor task");
            if (callback_ != nullptr)
                callback_(-1, false);
        }
    }

private:
    TaskHandle_t task_ = nullptr;
    Hcsr04ReadingCallback callback_ = nullptr;
    portMUX_TYPE isr_lock_ = portMUX_INITIALIZER_UNLOCKED;
    int64_t echo_rise_us_ = 0;
    int32_t echo_width_us_ = 0;
    bool waiting_for_echo_ = false;
#ifdef CONFIG_PM_ENABLE
    esp_pm_lock_handle_t sleep_lock_ = nullptr;
#endif
    std::array<int, 3> samples_ = {};
    size_t sample_count_ = 0;
    size_t sample_index_ = 0;

    static void EchoIsr(void* arg) {
        auto* self = static_cast<Hcsr04Sensor*>(arg);
        const int level = gpio_get_level(HCSR04_ECHO_PIN);
        const int64_t now = esp_timer_get_time();
        BaseType_t wake = pdFALSE;

        portENTER_CRITICAL_ISR(&self->isr_lock_);
        if (self->waiting_for_echo_) {
            if (level != 0) {
                self->echo_rise_us_ = now;
            } else if (self->echo_rise_us_ != 0) {
                const int64_t width = now - self->echo_rise_us_;
                if (width > 0 && width <= INT32_MAX) {
                    self->echo_width_us_ = static_cast<int32_t>(width);
                    self->waiting_for_echo_ = false;
                    vTaskNotifyGiveFromISR(self->task_, &wake);
                }
            }
        }
        portEXIT_CRITICAL_ISR(&self->isr_lock_);
        if (wake == pdTRUE)
            portYIELD_FROM_ISR();
    }

    void BeginMeasurement() {
        ulTaskNotifyTake(pdTRUE, 0);
        portENTER_CRITICAL(&isr_lock_);
        echo_rise_us_ = 0;
        echo_width_us_ = 0;
        waiting_for_echo_ = true;
        portEXIT_CRITICAL(&isr_lock_);

        gpio_set_level(HCSR04_TRIGGER_PIN, 1);
        esp_rom_delay_us(10);
        gpio_set_level(HCSR04_TRIGGER_PIN, 0);
    }

    int TakeEchoWidth() {
        portENTER_CRITICAL(&isr_lock_);
        waiting_for_echo_ = false;
        const int width = echo_width_us_;
        portEXIT_CRITICAL(&isr_lock_);
        return width;
    }

    int Filter(int distance_cm) {
        samples_[sample_index_] = distance_cm;
        sample_index_ = (sample_index_ + 1) % samples_.size();
        sample_count_ = std::min(sample_count_ + 1, samples_.size());
        if (sample_count_ < samples_.size())
            return distance_cm;
        auto ordered = samples_;
        std::sort(ordered.begin(), ordered.end());
        return ordered[1];
    }

    void Run() {
        TickType_t next_sample = xTaskGetTickCount();
        while (true) {
#ifdef CONFIG_PM_ENABLE
            ESP_ERROR_CHECK(esp_pm_lock_acquire(sleep_lock_));
#endif
            BeginMeasurement();
            const bool notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kEchoTimeoutMs)) != 0;
            const int echo_us = TakeEchoWidth();
#ifdef CONFIG_PM_ENABLE
            ESP_ERROR_CHECK(esp_pm_lock_release(sleep_lock_));
#endif
            const bool valid = notified && echo_us >= kMinEchoUs && echo_us <= kMaxEchoUs;
            if (callback_ != nullptr) {
                // Round the round-trip time to the nearest centimetre (sound travels 1 cm
                // out and back in approximately 58 us at room temperature).
                callback_(valid ? Filter((echo_us + 29) / 58) : -1, valid);
            }
            if (!valid)
                sample_count_ = 0;
            vTaskDelayUntil(&next_sample, pdMS_TO_TICKS(kSamplePeriodMs));
        }
    }
};

}  // namespace

void InitializeHcsr04Sensor(Hcsr04ReadingCallback callback) {
    Hcsr04Sensor::Instance().Start(callback);
}
