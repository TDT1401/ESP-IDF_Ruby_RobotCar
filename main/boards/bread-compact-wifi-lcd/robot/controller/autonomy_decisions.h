#pragma once

#include <algorithm>

namespace robot_autonomy {

enum class Motion { kStop, kForward, kBackward, kLeft, kRight };

constexpr Motion DecideFollowMotion(int distance_cm, int target_cm, bool reversing,
                                    int max_target_cm, int hysteresis_cm, int reverse_trigger_cm) {
    const int target = std::min(target_cm, max_target_cm);
    if (distance_cm > max_target_cm)
        return Motion::kStop;
    if (distance_cm < target && (distance_cm <= reverse_trigger_cm || reversing))
        return Motion::kBackward;
    return distance_cm <= target + hysteresis_cm ? Motion::kStop : Motion::kForward;
}

enum class AvoidPhase {
    kForward,
    kReverse,
    kTurnFirst,
    kMeasureFirst,
    kTurnSecond,
    kMeasureSecond,
    kReturnFirst,
    kMeasureChosen,
    kBlocked
};

enum class AvoidAction { kNone, kStop, kForward, kReverse, kTurnRight, kTurnLeft, kBlock };

constexpr AvoidAction DecideAvoidAction(AvoidPhase phase, int distance_cm, int stop_cm,
                                        int clear_cm, int first_clear_cm, int clear_samples,
                                        int first_distance_cm) {
    switch (phase) {
        case AvoidPhase::kBlocked:
            if (distance_cm <= clear_cm)
                return AvoidAction::kStop;
            return clear_samples >= 3 ? AvoidAction::kForward : AvoidAction::kNone;
        case AvoidPhase::kForward:
            return distance_cm <= stop_cm ? AvoidAction::kReverse : AvoidAction::kForward;
        case AvoidPhase::kMeasureFirst:
            return distance_cm > std::max(first_clear_cm, stop_cm) ? AvoidAction::kForward
                                                                   : AvoidAction::kTurnRight;
        case AvoidPhase::kMeasureSecond:
            if (std::max(first_distance_cm, distance_cm) <= stop_cm)
                return AvoidAction::kBlock;
            return distance_cm >= first_distance_cm ? AvoidAction::kForward
                                                    : AvoidAction::kTurnLeft;
        case AvoidPhase::kMeasureChosen:
            return distance_cm > stop_cm ? AvoidAction::kForward : AvoidAction::kBlock;
        default:
            return AvoidAction::kNone;
    }
}

static_assert(DecideFollowMotion(10, 30, false, 300, 4, 10) == Motion::kBackward);
static_assert(DecideFollowMotion(34, 30, false, 300, 4, 10) == Motion::kStop);
static_assert(DecideAvoidAction(AvoidPhase::kBlocked, 31, 20, 30, 40, 3, -1) ==
              AvoidAction::kForward);
static_assert(DecideAvoidAction(AvoidPhase::kMeasureSecond, 22, 20, 30, 40, 2, 25) ==
              AvoidAction::kTurnLeft);

}  // namespace robot_autonomy
