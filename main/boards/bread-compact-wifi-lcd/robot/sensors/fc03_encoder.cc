#include "fc03_encoder.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cinttypes>
#include <cmath>

#include "../../config.h"

namespace {
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
constexpr char kTag[] = "FC03";
#endif

bool AddMetric(cJSON* json, const char* key, double value, bool valid) {
    return valid ? cJSON_AddNumberToObject(json, key, value) != nullptr
                 : cJSON_AddNullToObject(json, key) != nullptr;
}

bool AddWheel(cJSON* parent, const char* name, const Fc03WheelStatus& wheel, bool valid) {
    auto* json = cJSON_AddObjectToObject(parent, name);
    return json && cJSON_AddNumberToObject(json, "pulses", wheel.pulses) &&
           cJSON_AddNumberToObject(json, "unknown_direction_pulses",
                                   wheel.unknown_direction_pulses) &&
           cJSON_AddNumberToObject(json, "hz", wheel.hz) &&
           cJSON_AddNumberToObject(json, "level", wheel.level) &&
           cJSON_AddNumberToObject(json, "command", wheel.command) &&
           cJSON_AddBoolToObject(json, "no_pulses", wheel.no_pulses) &&
           AddMetric(json, "rpm", wheel.rpm, valid) &&
           AddMetric(json, "distance_m", wheel.distance_m, valid);
}

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
constexpr int kCountLimit = 30000;
constexpr int64_t kRatePeriodUs = 500000;
constexpr int64_t kNoPulseTimeoutUs = 2000000;
constexpr int kPins[] = {FC03_LEFT_GPIO, FC03_RIGHT_GPIO};
constexpr double kPulsesPerRev[] = {FC03_LEFT_PULSES_PER_REV, FC03_RIGHT_PULSES_PER_REV};
constexpr double kDiameterMm[] = {FC03_LEFT_WHEEL_DIAMETER_MM, FC03_RIGHT_WHEEL_DIAMETER_MM};
constexpr double kPi = 3.14159265358979323846;
static_assert(FC03_LEFT_GPIO != FC03_RIGHT_GPIO);
static_assert(FC03_GLITCH_FILTER_NS > 0 && FC03_GLITCH_FILTER_NS <= 12000);
static_assert(FC03_COAST_MS >= 0 && FC03_COAST_MS <= 2000);
static_assert(kPulsesPerRev[0] >= 0 && kPulsesPerRev[0] <= 1000000 && kPulsesPerRev[1] >= 0 &&
              kPulsesPerRev[1] <= 1000000);
static_assert(kDiameterMm[0] >= 0 && kDiameterMm[0] <= 1000 && kDiameterMm[1] >= 0 &&
              kDiameterMm[1] <= 1000);
#endif
}  // namespace

bool AddFc03EncoderJson(cJSON* parent, const Fc03EncoderStatus& status) {
    auto* json = cJSON_AddObjectToObject(parent, "encoder");
    const bool valid = status.ready && status.calibrated;
    return json && cJSON_AddBoolToObject(json, "enabled", status.enabled) &&
           cJSON_AddBoolToObject(json, "ready", status.ready) &&
           cJSON_AddBoolToObject(json, "calibrated", status.calibrated) &&
           cJSON_AddBoolToObject(json, "distance_complete", status.distance_complete) &&
           cJSON_AddStringToObject(json, "direction_source", "motor_command") &&
           cJSON_AddStringToObject(json, "error", esp_err_to_name(status.error)) &&
           AddMetric(json, "travel_m", status.travel_m, valid) &&
           AddWheel(json, "left", status.left, valid) &&
           AddWheel(json, "right", status.right, valid);
}

Fc03Encoder::~Fc03Encoder() {
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
    ReleaseCounters();
#endif
}

void Fc03Encoder::Initialize() {
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
    if (status_.enabled)
        return;
    status_.enabled = true;
    status_.calibrated =
        kPulsesPerRev[0] > 0 && kPulsesPerRev[1] > 0 && kDiameterMm[0] > 0 && kDiameterMm[1] > 0;
    ESP_LOGI(kTag, "D0 left=GPIO%d right=GPIO%d; rising edges; filter=%d ns; coast=%d ms", kPins[0],
             kPins[1], FC03_GLITCH_FILTER_NS, FC03_COAST_MS);
    ESP_LOGI(kTag,
             "Calibration L: %.3f pulses/wheel-rev, %.3f mm; R: %.3f pulses/wheel-rev, %.3f mm",
             kPulsesPerRev[0], kDiameterMm[0], kPulsesPerRev[1], kDiameterMm[1]);
    for (int i = 0; i < 2; ++i) {
        status_.error = InitializeCounter(counters_[i], kPins[i]);
        if (status_.error != ESP_OK) {
            ESP_LOGE(kTag, "GPIO%d PCNT setup failed: %s; encoder telemetry unavailable", kPins[i],
                     esp_err_to_name(status_.error));
            ReleaseCounters();
            return;
        }
    }
    status_.ready = true;
    rate_at_us_ = esp_timer_get_time();
    if (!status_.calibrated) {
        ESP_LOGW(kTag,
                 "UNCALIBRATED: turn each wheel exactly 10 revolutions by hand; "
                 "(end total - start total)/10 = pulses per WHEEL revolution. "
                 "Set FC03_*_PULSES_PER_REV and FC03_*_WHEEL_DIAMETER_MM in config.h.");
    }
    ESP_LOGI(kTag,
             "Totals reset at reboot. Directions are inferred from commands, not measured. "
             "Power at 3V3, common GND, A0 unused.");
#endif
}

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
esp_err_t Fc03Encoder::InitializeCounter(Counter& counter, int gpio) {
    pcnt_unit_config_t unit = {};
    unit.low_limit = -kCountLimit;
    unit.high_limit = kCountLimit;
    unit.flags.accum_count = true;
    esp_err_t error = pcnt_new_unit(&unit, &counter.unit);
    if (error != ESP_OK)
        return error;
    pcnt_glitch_filter_config_t filter = {};
    filter.max_glitch_ns = FC03_GLITCH_FILTER_NS;
    if ((error = pcnt_unit_set_glitch_filter(counter.unit, &filter)) != ESP_OK)
        return error;
    pcnt_chan_config_t channel = {};
    channel.edge_gpio_num = gpio;
    channel.level_gpio_num = -1;
    if ((error = pcnt_new_channel(counter.unit, &channel, &counter.channel)) != ESP_OK)
        return error;
    // FC-03's comparator output may be open collector. Define idle high at 3.3 V.
    if ((error = gpio_set_pull_mode(static_cast<gpio_num_t>(gpio), GPIO_PULLUP_ONLY)) != ESP_OK ||
        (error = pcnt_channel_set_edge_action(counter.channel, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                              PCNT_CHANNEL_EDGE_ACTION_HOLD)) != ESP_OK ||
        (error = pcnt_channel_set_level_action(counter.channel, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                               PCNT_CHANNEL_LEVEL_ACTION_KEEP)) != ESP_OK)
        return error;
    if ((error = pcnt_unit_add_watch_point(counter.unit, kCountLimit)) != ESP_OK)
        return error;
    counter.high_watch = true;
    if ((error = pcnt_unit_add_watch_point(counter.unit, -kCountLimit)) != ESP_OK)
        return error;
    counter.low_watch = true;
    if ((error = pcnt_unit_enable(counter.unit)) != ESP_OK)
        return error;
    counter.enabled = true;
    if ((error = pcnt_unit_clear_count(counter.unit)) != ESP_OK)
        return error;
    return pcnt_unit_start(counter.unit);
}

void Fc03Encoder::ReleaseCounters() {
    for (auto& counter : counters_) {
        if (counter.enabled) {
            pcnt_unit_stop(counter.unit);
            pcnt_unit_disable(counter.unit);
        }
        if (counter.high_watch)
            pcnt_unit_remove_watch_point(counter.unit, kCountLimit);
        if (counter.low_watch)
            pcnt_unit_remove_watch_point(counter.unit, -kCountLimit);
        if (counter.channel)
            pcnt_del_channel(counter.channel);
        if (counter.unit)
            pcnt_del_unit(counter.unit);
        counter = {};
    }
}

void Fc03Encoder::FinishSegment() {
    completed_travel_m_ += std::abs(segment_centre_m_);
    segment_centre_m_ = 0;
}
#endif

void Fc03Encoder::Update(int64_t now_us) {
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
    if (!status_.ready)
        return;
    int counts[2] = {};
    // Read both before changing any baseline; never clear a live hardware counter.
    for (int i = 0; i < 2; ++i) {
        status_.error = pcnt_unit_get_count(counters_[i].unit, &counts[i]);
        if (status_.error != ESP_OK) {
            status_.ready = false;  // Latch telemetry failure until reboot, separate from motors.
            status_.distance_complete = false;
            return;
        }
    }
    Fc03WheelStatus* wheels[] = {&status_.left, &status_.right};
    const bool rate_due = now_us - rate_at_us_ >= kRatePeriodUs;
    bool expiring = false;
    for (int i = 0; i < 2; ++i) {
        if (wheels[i]->command == 0 && counters_[i].direction != 0 &&
            now_us >= counters_[i].coast_until_us) {
            expiring = true;
        }
    }
    if (expiring) {
        FinishSegment();
        for (int i = 0; i < 2; ++i) {
            if (wheels[i]->command == 0 && now_us >= counters_[i].coast_until_us)
                counters_[i].direction = 0;
        }
    }
    for (int i = 0; i < 2; ++i) {
        auto& counter = counters_[i];
        auto& wheel = *wheels[i];
        // Unsigned subtraction also handles the IDF accumulated 32-bit count wrapping.
        const uint32_t count = static_cast<uint32_t>(counts[i]);
        const uint32_t delta = count - counter.previous;
        counter.previous = count;
        wheel.pulses += delta;
        wheel.level = gpio_get_level(static_cast<gpio_num_t>(kPins[i]));
        if (delta != 0)
            counter.last_pulse_us = now_us;
        if (counter.direction == 0 && delta != 0) {
            wheel.unknown_direction_pulses += delta;
            status_.distance_complete = false;
        }
        wheel.no_pulses =
            wheel.command != 0 &&
            now_us - std::max(counter.drive_started_us, counter.last_pulse_us) >= kNoPulseTimeoutUs;
        if (status_.calibrated) {
            const double metres_per_pulse = kPi * kDiameterMm[i] / (1000.0 * kPulsesPerRev[i]);
            wheel.distance_m = wheel.pulses * metres_per_pulse;
            segment_centre_m_ += delta * metres_per_pulse * counter.direction / 2.0;
        }
        if (rate_due) {
            wheel.hz = (wheel.pulses - counter.rate_baseline) * 1000000.0 / (now_us - rate_at_us_);
            wheel.rpm = status_.calibrated ? wheel.hz * 60.0 / kPulsesPerRev[i] : 0;
            counter.rate_baseline = wheel.pulses;
        }
    }
    if (rate_due)
        rate_at_us_ = now_us;
    // Keep a signed sum for each direction segment. Taking abs on EVERY sample would
    // incorrectly add distance while spinning, because left/right pulses are asynchronous.
    status_.travel_m = completed_travel_m_ + std::abs(segment_centre_m_);
#else
    (void)now_us;
#endif
}

void Fc03Encoder::SetDrive(int left, int right, int64_t now_us) {
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
    Update(now_us);  // Attribute already counted pulses to the OLD direction.
    const int commands[] = {(left > 0) - (left < 0), (right > 0) - (right < 0)};
    const int centre_command = (left + right > 0) - (left + right < 0);
    // A joystick can reverse centre motion while the two wheels still counter-rotate.
    if ((left != 0 || right != 0) && centre_command != centre_command_) {
        FinishSegment();
        centre_command_ = centre_command;
    }
    Fc03WheelStatus* wheels[] = {&status_.left, &status_.right};
    for (int i = 0; i < 2; ++i) {
        auto& wheel = *wheels[i];
        auto& counter = counters_[i];
        if (wheel.command == commands[i])
            continue;
        wheel.command = commands[i];
        counter.drive_started_us = now_us;
        wheel.no_pulses = false;
        if (commands[i] == 0) {
            counter.coast_until_us = now_us + FC03_COAST_MS * 1000LL;
        } else if (counter.direction != commands[i]) {
            FinishSegment();
            counter.direction = commands[i];
        }
    }
#else
    (void)left;
    (void)right;
    (void)now_us;
#endif
}

void StartFc03Monitor(Fc03EncoderStatus (*snapshot)()) {
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
    static Fc03EncoderStatus (*read_status)() = snapshot;
    static TaskHandle_t task = nullptr;
    if (task != nullptr)
        return;
    const auto result = xTaskCreate(
        [](void*) {
#if FC03_DEBUG
            uint64_t previous_left = 0, previous_right = 0;
            int64_t previous_at = esp_timer_get_time();
#endif
            while (true) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                const auto status = read_status();  // Locks only while copying a snapshot.
                if (!status.ready) {
                    ESP_LOGE(kTag, "Encoder unavailable: %s", esp_err_to_name(status.error));
                    vTaskDelete(nullptr);
                    return;
                }
#if FC03_DEBUG
                const int64_t now = esp_timer_get_time();
                ESP_LOGI(kTag,
                         "dt=%.2fs D0 L/R=%d/%d cmd=%d/%d total=%" PRIu64 "/%" PRIu64
                         " delta=%" PRIu64 "/%" PRIu64 " Hz=%.1f/%.1f",
                         (now - previous_at) / 1000000.0, status.left.level, status.right.level,
                         status.left.command, status.right.command, status.left.pulses,
                         status.right.pulses, status.left.pulses - previous_left,
                         status.right.pulses - previous_right, status.left.hz, status.right.hz);
                if (status.calibrated) {
                    ESP_LOGI(kTag,
                             "RPM L/R=%.1f/%.1f wheel_m=%.3f/%.3f travel_est_m=%.3f complete=%d",
                             status.left.rpm, status.right.rpm, status.left.distance_m,
                             status.right.distance_m, status.travel_m, status.distance_complete);
                } else {
                    ESP_LOGI(kTag,
                             "UNCALIBRATED: metres/RPM unavailable; set the 4 FC03 calibration "
                             "values in config.h. Use total differences to count wheel turns.");
                }
                if (status.left.no_pulses || status.right.no_pulses) {
                    ESP_LOGW(kTag,
                             "No pulses for >=2s while commanded: left=%d right=%d. Check D0, "
                             "disc alignment, sensitivity (if adjustable), or stalled motor.",
                             status.left.no_pulses, status.right.no_pulses);
                }
                if (!status.distance_complete) {
                    ESP_LOGW(kTag,
                             "Unknown-direction pulses L/R=%" PRIu64 "/%" PRIu64
                             "; wheel totals include them, robot travel excludes them. "
                             "Reboot after manual calibration before a driving test.",
                             status.left.unknown_direction_pulses,
                             status.right.unknown_direction_pulses);
                }
                previous_left = status.left.pulses;
                previous_right = status.right.pulses;
                previous_at = now;
#endif
            }
        },
        "fc03_monitor", 3072, nullptr, 1, &task);
    if (result != pdPASS) {
        task = nullptr;
        ESP_LOGE(kTag, "Could not start monitor logging task; telemetry still available over HTTP");
    }
#else
    (void)snapshot;
#endif
}
