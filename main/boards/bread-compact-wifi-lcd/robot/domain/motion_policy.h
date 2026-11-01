#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace ruby_robot {

constexpr int kReferenceSpeedPercent = 50;

enum class Motion { kStop, kForward, kBackward, kLeft, kRight };

struct WheelSpeeds {
    int left;
    int right;
};

inline WheelSpeeds MixJoystick(int x, int y, int speed_percent) {
    int left = y + x;
    int right = y - x;
    const int peak = std::max(std::abs(left), std::abs(right));
    if (peak > 1000) {
        left = left * 1000 / peak;
        right = right * 1000 / peak;
    }
    return {left * speed_percent / 1000, right * speed_percent / 1000};
}

inline int ScaleDistanceForSpeed(int distance_cm, int speed_percent) {
    const int scaled_speed = std::max(speed_percent, kReferenceSpeedPercent);
    return (distance_cm * scaled_speed + kReferenceSpeedPercent - 1) / kReferenceSpeedPercent;
}

inline bool IsSleepEmotion(const char* emotion) {
    return emotion != nullptr && (strcmp(emotion, "sleep") == 0 || strcmp(emotion, "sleepy") == 0);
}

inline const char* MotionName(Motion motion) {
    switch (motion) {
        case Motion::kForward:
            return "forward";
        case Motion::kBackward:
            return "backward";
        case Motion::kLeft:
            return "turn_left";
        case Motion::kRight:
            return "turn_right";
        default:
            return "stopped";
    }
}

}  // namespace ruby_robot
