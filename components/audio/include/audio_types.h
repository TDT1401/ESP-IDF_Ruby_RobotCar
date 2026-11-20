#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct AudioStreamPacket {
    int sample_rate = 0;
    int frame_duration = 0;
    uint32_t timestamp = 0;
    uint32_t playback_id = 0;
    uint32_t media_position_ms = 0;
    std::vector<uint8_t> payload;
};

// Reads borrowed asset data during engine initialization; the caller owns the data.
using AudioAssetReader = std::function<bool(const std::string& name, void*& data, size_t& size)>;
