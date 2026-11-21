#pragma once

#include <algorithm>

namespace ruby_robot {

constexpr int kReferenceSpeedPercent = 50;

enum class Motion { kStop, kForward, kBackward, kLeft, kRight };

inline int ScaleDistanceForSpeed(int distance_cm, int speed_percent) {
    const int scaled_speed = std::max(speed_percent, kReferenceSpeedPercent);
    return (distance_cm * scaled_speed + kReferenceSpeedPercent - 1) / kReferenceSpeedPercent;
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
