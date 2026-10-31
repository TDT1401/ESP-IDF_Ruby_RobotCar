#pragma once

#include <cstdint>

namespace robot_eye_detail {
inline constexpr uint32_t kAnimationPeriodMs = 33;
inline constexpr uint32_t kInfoScreenDurationMs = 15000;
inline constexpr uint32_t kInfoRefreshPeriodMs = 500;
inline constexpr uint32_t kWeatherFramePeriodMs = 100;
inline constexpr uint32_t kMorphDurationMs = 500;
inline constexpr uint32_t kBlinkCloseMs = 200;
inline constexpr uint32_t kBlinkHoldMinMs = 30;
inline constexpr uint32_t kBlinkHoldMaxMs = 60;
inline constexpr uint32_t kBlinkOpenMs = 100;
inline constexpr uint32_t kBlinkIntervalMinMs = 7000;
inline constexpr uint32_t kBlinkIntervalMaxMs = 12000;
inline constexpr uint32_t kHalfBlinkPhaseMs = 150;
inline constexpr uint32_t kHalfBlinkIntervalMinMs = 4000;
inline constexpr uint32_t kHalfBlinkIntervalMaxMs = 8000;
inline constexpr uint32_t kGazeTransitionMs = 600;
inline constexpr uint32_t kGazeHoldMinMs = 1000;
inline constexpr uint32_t kGazeHoldMaxMs = 2000;
inline constexpr uint32_t kGazeIntervalMinMs = 4000;
inline constexpr uint32_t kGazeIntervalMaxMs = 7000;
inline constexpr uint32_t kDriftIntervalMs = 5000;
inline constexpr uint32_t kBootClosedMs = 500;
inline constexpr uint32_t kBootToHalfMs = 1500;
inline constexpr uint32_t kBootHalfBlinkGapMs = 300;
inline constexpr uint32_t kBootToFullMs = 1500;
inline constexpr int kBootBlinkCount = 3;
inline constexpr int kReferenceEyeWidth = 32;
inline constexpr int kReferenceEyeHeight = 32;
inline constexpr int kReferenceEyeGap = 8;
inline constexpr int kMaxFullRefreshPixels = 160 * 80;
}  // namespace robot_eye_detail
