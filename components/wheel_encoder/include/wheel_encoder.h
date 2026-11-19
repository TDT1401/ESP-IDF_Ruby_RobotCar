#pragma once

#include <esp_err.h>
#include <soc/soc_caps.h>

#include <array>
#include <cstdint>

#if SOC_PCNT_SUPPORTED
#include <driver/pulse_cnt.h>
#endif

struct WheelEncoderConfig {
    bool enabled = false;
    std::array<int, 2> gpio = {-1, -1};
    std::array<double, 2> pulses_per_rev = {};
    std::array<double, 2> wheel_diameter_mm = {};
    int glitch_filter_ns = 0;
    int coast_ms = 0;
};

struct Fc03WheelStatus {
    uint64_t pulses = 0;
    uint64_t unknown_direction_pulses = 0;
    double hz = 0;
    double rpm = 0;
    double distance_m = 0;
    double signed_distance_m = 0;  // Command-inferred wheel direction; for local odometry.
    int level = -1;
    int command = 0;         // Logical wheel direction, before motor wiring inversion: -1/0/+1.
    bool no_pulses = false;  // Advisory only; cannot distinguish a stall from a loose wire.
};

struct Fc03EncoderStatus {
    bool enabled = false;
    bool ready = false;
    bool calibrated = false;
    bool distance_complete = true;
    double travel_m = 0;  // Estimated robot-centre path length since boot, including reverse.
    Fc03WheelStatus left;
    Fc03WheelStatus right;
    esp_err_t error = ESP_OK;
};

// Owned by the board-lifetime motor controller. All runtime methods are serialized by
// its mutex. Update/SetDrive never log, allocate, sleep or perform network operations.
class Fc03Encoder {
public:
    explicit Fc03Encoder(const WheelEncoderConfig& config) : config_(config) {}
    ~Fc03Encoder();
    Fc03Encoder(const Fc03Encoder&) = delete;
    Fc03Encoder& operator=(const Fc03Encoder&) = delete;

    void Initialize();
    void Update(int64_t now_us);
    void SetDrive(int left, int right, int64_t now_us);
    Fc03EncoderStatus Snapshot() const { return status_; }

private:
    WheelEncoderConfig config_;
    Fc03EncoderStatus status_;
#if SOC_PCNT_SUPPORTED
    struct Counter {
        pcnt_unit_handle_t unit = nullptr;
        pcnt_channel_handle_t channel = nullptr;
        bool enabled = false;
        bool high_watch = false;
        bool low_watch = false;
        uint32_t previous = 0;
        int direction = 0;
        int64_t coast_until_us = 0;
        int64_t drive_started_us = 0;
        int64_t last_pulse_us = 0;
        int64_t rate_decay_at_us = 0;
        int64_t rate_stale_at_us = 0;
    };
    Counter counters_[2];
    double completed_travel_m_ = 0;
    double segment_centre_m_ = 0;
    int centre_command_ = 0;

    esp_err_t InitializeCounter(Counter& counter, int gpio);
    void ReleaseCounters();
    void FinishSegment();
#endif
};
