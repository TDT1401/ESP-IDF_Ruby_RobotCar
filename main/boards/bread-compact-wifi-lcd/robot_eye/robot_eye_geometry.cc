#include "robot_eye_display.h"

#include <algorithm>
#include <cmath>

#include <esp_random.h>
#include <esp_timer.h>

#include "robot_eye_constants.h"

using namespace robot_eye_detail;

float RobotEyeDisplay::Clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

float RobotEyeDisplay::EaseInOut(float value) {
    if (value < 0.5f) {
        return 2.0f * value * value;
    }
    const float remainder = -2.0f * value + 2.0f;
    return 1.0f - remainder * remainder * 0.5f;
}

int16_t RobotEyeDisplay::Lerp(int16_t from, int16_t to, float amount) {
    return static_cast<int16_t>(from + static_cast<float>(to - from) * amount);
}

uint32_t RobotEyeDisplay::NowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

uint32_t RobotEyeDisplay::Random(uint32_t minimum, uint32_t maximum) {
    return maximum > minimum ? minimum + esp_random() % (maximum - minimum) : minimum;
}

int16_t RobotEyeDisplay::Scale(int16_t value) const {
    return static_cast<int16_t>(value * geometry_scale_);
}

RobotEyeDisplay::FaceParams RobotEyeDisplay::GetBaseFace() const {
    const int16_t drift_x = static_cast<int16_t>(drift_x_);
    const int16_t drift_y = static_cast<int16_t>(drift_y_);
    const EyeParams left = {static_cast<int16_t>(base_left_x_ + drift_x),
                            static_cast<int16_t>(base_y_ + drift_y),
                            Scale(32),
                            Scale(32),
                            Scale(10),
                            0,
                            0,
                            0,
                            0};
    const EyeParams right = {static_cast<int16_t>(base_right_x_ + drift_x),
                             static_cast<int16_t>(base_y_ + drift_y),
                             Scale(32),
                             Scale(32),
                             Scale(10),
                             0,
                             0,
                             0,
                             0};
    return {left, right};
}

RobotEyeDisplay::FaceParams RobotEyeDisplay::GetExpressionFace(Expression expression) const {
    FaceParams face = GetBaseFace();
    const int16_t left_center_y = face.left.y + face.left.height / 2;
    const int16_t right_center_y = face.right.y + face.right.height / 2;
    switch (expression) {
        case Expression::kHappy:
        case Expression::kExcited:
            face.left.bottom_cut = face.right.bottom_cut = Scale(20);
            break;
        case Expression::kAngry:
        case Expression::kFurious: {
            const int16_t cut = expression == Expression::kFurious ? Scale(11) : Scale(10);
            const int16_t slope = Scale(8);
            face.left.top_slope_left = cut;
            face.left.top_slope_right = cut + slope;
            face.right.top_slope_left = cut + slope;
            face.right.top_slope_right = cut;
            break;
        }
        case Expression::kSad:
            face.left.y += Scale(12);
            face.right.y += Scale(12);
            face.left.top_slope_left = face.right.top_slope_right = Scale(24);
            face.left.top_slope_right = face.right.top_slope_left = Scale(16);
            break;
        case Expression::kSleepy:
            face.left.top_slope_left = face.right.top_slope_right = Scale(26);
            face.left.top_slope_right = face.right.top_slope_left = Scale(22);
            break;
        case Expression::kSurprised:
            face.left.x += Scale(3);
            face.right.x -= Scale(3);
            break;
        case Expression::kScared:
        case Expression::kWorried:
            if (expression == Expression::kScared) {
                face.left.y -= Scale(6);
                face.right.y -= Scale(6);
                face.left.radius = face.right.radius = 0;
            } else {
                face.left.radius = face.right.radius = Scale(8);
            }
            face.left.top_slope_left = face.right.top_slope_right = Scale(5);
            break;
        case Expression::kDespair:
            face.left.y -= Scale(6);
            face.right.y -= Scale(6);
            face.left.height = face.right.height = Scale(16);
            face.left.radius = face.right.radius = 0;
            face.left.top_slope_left = face.right.top_slope_right = Scale(5);
            break;
        case Expression::kFocused:
            face.left.height = face.right.height = Scale(16);
            face.left.radius = face.right.radius = Scale(5);
            face.left.y = left_center_y - face.left.height / 2;
            face.right.y = right_center_y - face.right.height / 2;
            break;
        case Expression::kAlert:
            face.left.width = face.right.width = Scale(8);
            face.left.height = face.right.height = Scale(36);
            face.left.radius = face.right.radius = Scale(4);
            face.left.x += Scale(12);
            face.right.x += Scale(12);
            face.left.y = left_center_y - face.left.height / 2;
            face.right.y = right_center_y - face.right.height / 2;
            break;
        case Expression::kBlink:
        case Expression::kBlinkDown:
        case Expression::kBlinkUp: {
            const int16_t offset = expression == Expression::kBlinkDown
                                       ? Scale(10)
                                       : (expression == Expression::kBlinkUp ? -Scale(10) : 0);
            face.left.height = face.right.height = Scale(4);
            face.left.radius = face.right.radius = Scale(2);
            face.left.y = left_center_y - Scale(2) + offset;
            face.right.y = right_center_y - Scale(2) + offset;
            break;
        }
        case Expression::kDisoriented:
            face.left.width = face.right.width = Scale(30);
            face.left.height = face.right.height = Scale(30);
            face.left.radius = face.right.radius = Scale(12);
            face.left.x -= Scale(4);
            face.right.x += Scale(6);
            face.left.y = left_center_y - Scale(15);
            face.right.y = right_center_y - Scale(15);
            break;
        case Expression::kLookDown:
            face.left.y += Scale(10);
            face.right.y += Scale(10);
            break;
        case Expression::kLookUp:
            face.left.y -= Scale(10);
            face.right.y -= Scale(10);
            break;
        case Expression::kLookLeft:
            face.left.x -= Scale(18);
            face.left.y = left_center_y - Scale(21);
            face.left.width = face.left.height = Scale(41);
            face.left.radius = Scale(13);
            face.right.x -= Scale(14);
            break;
        case Expression::kLookRight:
            face.right.x += Scale(10);
            face.right.y = right_center_y - Scale(21);
            face.right.width = face.right.height = Scale(41);
            face.right.radius = Scale(13);
            face.left.x += Scale(14);
            break;
        case Expression::kWinkLeft:
            face.left.height = Scale(4);
            face.left.radius = Scale(2);
            face.left.y = left_center_y - Scale(2);
            break;
        case Expression::kWinkRight:
            face.right.height = Scale(4);
            face.right.radius = Scale(2);
            face.right.y = right_center_y - Scale(2);
            break;
        case Expression::kBored:
            face.left.y += Scale(20);
            face.right.y += Scale(20);
            face.left.height = face.right.height = Scale(14);
            face.left.radius = face.right.radius = Scale(7);
            face.left.top_cut = face.right.top_cut = Scale(7);
            break;
        default:
            break;
    }
    return face;
}

RobotEyeDisplay::FaceParams RobotEyeDisplay::InterpolateFace(const FaceParams& from,
                                                             const FaceParams& to,
                                                             float amount) const {
    const auto interpolate_eye = [amount](const EyeParams& first, const EyeParams& second) {
        return EyeParams{Lerp(first.x, second.x, amount),
                         Lerp(first.y, second.y, amount),
                         Lerp(first.width, second.width, amount),
                         Lerp(first.height, second.height, amount),
                         Lerp(first.radius, second.radius, amount),
                         Lerp(first.top_cut, second.top_cut, amount),
                         Lerp(first.bottom_cut, second.bottom_cut, amount),
                         Lerp(first.top_slope_left, second.top_slope_left, amount),
                         Lerp(first.top_slope_right, second.top_slope_right, amount)};
    };
    return {interpolate_eye(from.left, to.left), interpolate_eye(from.right, to.right)};
}

RobotEyeDisplay::FaceParams RobotEyeDisplay::ApplyBlink(const FaceParams& face, float amount,
                                                        bool half_blink) const {
    if (amount <= 0.001f) {
        return face;
    }
    FaceParams result = face;
    const float minimum_ratio = half_blink ? 0.70f : 0.06f;
    const float height_scale = 1.0f - amount * (1.0f - minimum_ratio);
    const int16_t expansion = half_blink ? 0 : static_cast<int16_t>(Scale(4) * amount);
    const auto transform_eye = [&](const EyeParams& source, EyeParams& target) {
        const int16_t center_y = source.y + source.height / 2;
        target.height = std::max<int16_t>(Scale(2), source.height * height_scale);
        target.y = center_y - target.height / 2;
        target.x = source.x - expansion;
        target.width = source.width + expansion * 2;
        target.radius = static_cast<int16_t>(source.radius * height_scale);
        target.top_cut = static_cast<int16_t>(source.top_cut * height_scale);
        target.bottom_cut = static_cast<int16_t>(source.bottom_cut * height_scale);
        target.top_slope_left = static_cast<int16_t>(source.top_slope_left * height_scale);
        target.top_slope_right = static_cast<int16_t>(source.top_slope_right * height_scale);
    };
    transform_eye(face.left, result.left);
    transform_eye(face.right, result.right);
    return result;
}

RobotEyeDisplay::FaceParams RobotEyeDisplay::ComputeFrame(uint32_t now) const {
    FaceParams face;
    if (transition_state_ == TransitionState::kMorph) {
        float amount = Clamp01(static_cast<float>(now - transition_started_ms_) / kMorphDurationMs);
        amount = EaseInOut(amount);
        face = InterpolateFace(GetExpressionFace(transition_from_),
                               GetExpressionFace(transition_to_), amount);
    } else if (transition_state_ == TransitionState::kBlinkSwitch &&
               blink_state_ == BlinkState::kOpening) {
        float amount = Clamp01(static_cast<float>(now - blink_started_ms_) / kBlinkOpenMs);
        face = InterpolateFace(GetExpressionFace(transition_from_),
                               GetExpressionFace(transition_to_), EaseInOut(amount));
    } else if (gaze_state_ == GazeState::kToDirection) {
        float amount = Clamp01(static_cast<float>(now - gaze_started_ms_) / kGazeTransitionMs);
        face = InterpolateFace(GetExpressionFace(Expression::kNormal),
                               GetExpressionFace(gaze_expression_), EaseInOut(amount));
    } else if (gaze_state_ == GazeState::kHolding) {
        face = GetExpressionFace(gaze_expression_);
    } else if (gaze_state_ == GazeState::kToCenter) {
        float amount = Clamp01(static_cast<float>(now - gaze_started_ms_) / kGazeTransitionMs);
        face = InterpolateFace(GetExpressionFace(gaze_expression_),
                               GetExpressionFace(Expression::kNormal), EaseInOut(amount));
    } else if (boot_phase_ == BootPhase::kClosed) {
        const EyeParams left = {base_left_x_,
                                static_cast<int16_t>(base_y_ + Scale(15)),
                                Scale(32),
                                Scale(2),
                                Scale(1),
                                0,
                                0,
                                0,
                                0};
        const EyeParams right = {base_right_x_,
                                 static_cast<int16_t>(base_y_ + Scale(15)),
                                 Scale(32),
                                 Scale(2),
                                 Scale(1),
                                 0,
                                 0,
                                 0,
                                 0};
        face = {left, right};
    } else if (boot_phase_ == BootPhase::kToHalf || boot_phase_ == BootPhase::kToFull) {
        const int16_t half_height = Scale(22);
        FaceParams half = GetBaseFace();
        half.left.height = half.right.height = half_height;
        half.left.radius = half.right.radius = Scale(7);
        half.left.y = half.right.y = base_y_ + (Scale(32) - half_height) / 2;
        if (boot_phase_ == BootPhase::kToHalf) {
            FaceParams closed = half;
            closed.left.height = closed.right.height = Scale(2);
            closed.left.radius = closed.right.radius = Scale(1);
            closed.left.y = closed.right.y = base_y_ + Scale(15);
            float amount =
                Clamp01(static_cast<float>(now - boot_phase_started_ms_) / kBootToHalfMs);
            face = InterpolateFace(closed, half, EaseInOut(amount));
        } else {
            float amount =
                Clamp01(static_cast<float>(now - boot_phase_started_ms_) / kBootToFullMs);
            face = InterpolateFace(half, GetBaseFace(), EaseInOut(amount));
        }
    } else if (boot_phase_ == BootPhase::kHalfBlink) {
        face = GetBaseFace();
        face.left.height = face.right.height = Scale(22);
        face.left.radius = face.right.radius = Scale(7);
        face.left.y = face.right.y = base_y_ + Scale(5);
    } else {
        face = GetExpressionFace(current_expression_);
    }

    float blink_amount = 0.0f;
    if (blink_state_ == BlinkState::kClosing) {
        blink_amount =
            EaseInOut(Clamp01(static_cast<float>(now - blink_started_ms_) / kBlinkCloseMs));
    } else if (blink_state_ == BlinkState::kHold) {
        blink_amount = 1.0f;
    } else if (blink_state_ == BlinkState::kOpening) {
        blink_amount =
            1.0f - EaseInOut(Clamp01(static_cast<float>(now - blink_started_ms_) / kBlinkOpenMs));
    }
    face = ApplyBlink(face, blink_amount, false);

    float half_blink_amount = 0.0f;
    if (half_blink_state_ == HalfBlinkState::kShrinking) {
        half_blink_amount = EaseInOut(
            Clamp01(static_cast<float>(now - half_blink_started_ms_) / kHalfBlinkPhaseMs));
    } else if (half_blink_state_ == HalfBlinkState::kExpanding) {
        half_blink_amount =
            1.0f - EaseInOut(Clamp01(static_cast<float>(now - half_blink_started_ms_) /
                                     kHalfBlinkPhaseMs));
    }
    return ApplyBlink(face, half_blink_amount, true);
}
