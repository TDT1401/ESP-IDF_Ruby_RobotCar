#include "wheel_encoder.h"

#include <driver/gpio.h>
#include <esp_log.h>

#include <algorithm>
#include <cmath>

namespace {
#if SOC_PCNT_SUPPORTED
constexpr char kTag[] = "FC03";
#endif

#if SOC_PCNT_SUPPORTED
constexpr int kCountLimit = 30000;
constexpr int64_t kNoPulseTimeoutUs = 2000000;
constexpr int64_t kRateStaleMinimumUs = 250000;
constexpr double kPi = 3.14159265358979323846;
#endif
}  // namespace

Fc03Encoder::~Fc03Encoder() {
#if SOC_PCNT_SUPPORTED
    ReleaseCounters();
#endif
}

void Fc03Encoder::Initialize() {
#if SOC_PCNT_SUPPORTED
    if (!config_.enabled || status_.enabled)
        return;
    status_.enabled = true;
    status_.calibrated = config_.pulses_per_rev[0] > 0 && config_.pulses_per_rev[1] > 0 &&
                         config_.wheel_diameter_mm[0] > 0 && config_.wheel_diameter_mm[1] > 0;
    ESP_LOGI(kTag, "D0 left=GPIO%d right=GPIO%d; rising edges; filter=%d ns; coast=%d ms",
             config_.gpio[0], config_.gpio[1], config_.glitch_filter_ns, config_.coast_ms);
    ESP_LOGI(kTag,
             "Calibration L: %.3f pulses/wheel-rev, %.3f mm; R: %.3f pulses/wheel-rev, %.3f mm",
             config_.pulses_per_rev[0], config_.wheel_diameter_mm[0], config_.pulses_per_rev[1],
             config_.wheel_diameter_mm[1]);
    for (int i = 0; i < 2; ++i) {
        status_.error = InitializeCounter(counters_[i], config_.gpio[i]);
        if (status_.error != ESP_OK) {
            ESP_LOGE(kTag, "GPIO%d PCNT setup failed: %s; encoder telemetry unavailable",
                     config_.gpio[i], esp_err_to_name(status_.error));
            ReleaseCounters();
            return;
        }
    }
    status_.ready = true;
    if (!status_.calibrated) {
        ESP_LOGW(kTag,
                 "UNCALIBRATED: turn each wheel exactly 10 revolutions by hand; "
                 "(end total - start total)/10 = pulses per WHEEL revolution. "
                 "Set FC03_*_PULSES_PER_REV and FC03_*_WHEEL_DIAMETER_MM in "
                 "config/robot_hardware_config.h.");
    }
    ESP_LOGI(kTag,
             "Totals reset at reboot. Directions are inferred from commands, not measured. "
             "Power at 3V3, common GND, A0 unused.");
#endif
}

#if SOC_PCNT_SUPPORTED
esp_err_t Fc03Encoder::InitializeCounter(Counter& counter, int gpio) {
    pcnt_unit_config_t unit = {};
    unit.low_limit = -kCountLimit;
    unit.high_limit = kCountLimit;
    unit.flags.accum_count = true;
    esp_err_t error = pcnt_new_unit(&unit, &counter.unit);
    if (error != ESP_OK)
        return error;
    pcnt_glitch_filter_config_t filter = {};
    filter.max_glitch_ns = config_.glitch_filter_ns;
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
#if SOC_PCNT_SUPPORTED
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
        wheel.level = gpio_get_level(static_cast<gpio_num_t>(config_.gpio[i]));
        if (delta != 0) {
            if (counter.last_pulse_us > 0 && now_us > counter.last_pulse_us) {
                const double sample_hz = delta * 1000000.0 / (now_us - counter.last_pulse_us);
                wheel.hz = wheel.hz > 0 ? wheel.hz * 0.5 + sample_hz * 0.5 : sample_hz;
            }
            counter.last_pulse_us = now_us;
            if (wheel.hz > 0) {
                counter.rate_decay_at_us =
                    now_us + std::max<int64_t>(100000, static_cast<int64_t>(2000000.0 / wheel.hz));
                counter.rate_stale_at_us =
                    now_us +
                    std::max(kRateStaleMinimumUs, static_cast<int64_t>(3000000.0 / wheel.hz));
            }
        } else if (counter.last_pulse_us > 0 && wheel.hz > 0) {
            const int64_t elapsed_us = now_us - counter.last_pulse_us;
            if (now_us >= counter.rate_stale_at_us) {
                wheel.hz = 0;
            } else if (now_us >= counter.rate_decay_at_us) {
                // A missing expected edge lowers the estimate before it becomes stale.
                wheel.hz = std::min(wheel.hz, 1000000.0 / elapsed_us);
            }
        }
        if (counter.direction == 0 && delta != 0) {
            wheel.unknown_direction_pulses += delta;
            status_.distance_complete = false;
        }
        wheel.no_pulses =
            wheel.command != 0 &&
            now_us - std::max(counter.drive_started_us, counter.last_pulse_us) >= kNoPulseTimeoutUs;
        if (status_.calibrated) {
            const double metres_per_pulse =
                kPi * config_.wheel_diameter_mm[i] / (1000.0 * config_.pulses_per_rev[i]);
            wheel.distance_m = wheel.pulses * metres_per_pulse;
            wheel.signed_distance_m += delta * metres_per_pulse * counter.direction;
            segment_centre_m_ += delta * metres_per_pulse * counter.direction / 2.0;
        }
        wheel.rpm = status_.calibrated ? wheel.hz * 60.0 / config_.pulses_per_rev[i] : 0;
    }
    // Keep a signed sum for each direction segment. Taking abs on EVERY sample would
    // incorrectly add distance while spinning, because left/right pulses are asynchronous.
    status_.travel_m = completed_travel_m_ + std::abs(segment_centre_m_);
#else
    (void)now_us;
#endif
}

void Fc03Encoder::SetDrive(int left, int right, int64_t now_us) {
#if SOC_PCNT_SUPPORTED
    if (!config_.enabled)
        return;
    Update(now_us);  // Attribute already counted pulses to the OLD direction.
    const int commands[] = {(left > 0) - (left < 0), (right > 0) - (right < 0)};
    const int centre_command = (left + right > 0) - (left + right < 0);
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
            counter.coast_until_us = now_us + config_.coast_ms * 1000LL;
        } else {
            counter.last_pulse_us = 0;
            counter.rate_decay_at_us = 0;
            counter.rate_stale_at_us = 0;
            wheel.hz = 0;
            wheel.rpm = 0;
            if (counter.direction != commands[i]) {
                FinishSegment();
                counter.direction = commands[i];
            }
        }
    }
#else
    (void)left;
    (void)right;
    (void)now_us;
#endif
}
