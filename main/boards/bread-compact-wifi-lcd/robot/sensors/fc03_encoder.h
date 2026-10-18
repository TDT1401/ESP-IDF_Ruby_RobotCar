#pragma once

#include <esp_err.h>
#include <cJSON.h>
#include <sdkconfig.h>

#include <cstdint>

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
#include <driver/pulse_cnt.h>
#endif

struct Fc03WheelStatus {
    uint64_t pulses = 0;
    uint64_t unknown_direction_pulses = 0;
    double hz = 0;
    double rpm = 0;
    double distance_m = 0;
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
    Fc03Encoder() = default;
    ~Fc03Encoder();
    Fc03Encoder(const Fc03Encoder&) = delete;
    Fc03Encoder& operator=(const Fc03Encoder&) = delete;

    void Initialize();
    void Update(int64_t now_us);
    void SetDrive(int left, int right, int64_t now_us);
    Fc03EncoderStatus Snapshot() const { return status_; }

private:
    Fc03EncoderStatus status_;
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
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

// Adds one owned "encoder" object. Distances/RPM are JSON null until calibrated/ready.
bool AddFc03EncoderJson(cJSON* parent, const Fc03EncoderStatus& status);

// Logging runs in a separate low-priority task, outside motor timer/locks/audio tasks.
void StartFc03Monitor(Fc03EncoderStatus (*snapshot)());
