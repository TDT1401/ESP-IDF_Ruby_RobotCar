#include "robot_eye_display.h"

#include <cstring>

#include <sdkconfig.h>

#include "robot_eye_constants.h"

using namespace robot_eye_detail;

namespace {
bool DeadlineReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}
}  // namespace

void RobotEyeDisplay::SetRobotEyeExpression(Expression expression, bool blink_switch) {
    if (expression == current_expression_ && transition_state_ == TransitionState::kIdle) {
        return;
    }
    const uint32_t now = NowMs();
    boot_phase_ = BootPhase::kIdle;
    half_blink_state_ = HalfBlinkState::kIdle;
    gaze_state_ = GazeState::kIdle;
    transition_from_ = current_expression_;
    transition_to_ = expression;
    if (blink_switch) {
        transition_state_ = TransitionState::kBlinkSwitch;
        StartBlink(now);
    } else {
        transition_state_ = TransitionState::kMorph;
        transition_started_ms_ = now;
    }
    next_blink_ms_ = now + RandomBlinkInterval();
    next_gaze_ms_ = now + Random(kGazeIntervalMinMs, kGazeIntervalMaxMs);
    next_half_blink_ms_ = now + Random(kHalfBlinkIntervalMinMs, kHalfBlinkIntervalMaxMs);
    next_drift_ms_ = now + kDriftIntervalMs;
}

void RobotEyeDisplay::StartBlink(uint32_t now) {
    blink_state_ = BlinkState::kClosing;
    blink_started_ms_ = now;
}

uint32_t RobotEyeDisplay::RandomBlinkInterval() const {
    if (current_expression_ == Expression::kAngry || current_expression_ == Expression::kHappy) {
        return Random(2000, 6001);
    }
    return Random(kBlinkIntervalMinMs, kBlinkIntervalMaxMs + 1);
}

void RobotEyeDisplay::UpdateBoot(uint32_t now) {
    switch (boot_phase_) {
        case BootPhase::kClosed:
            if (now - boot_phase_started_ms_ >= kBootClosedMs) {
                boot_phase_ = BootPhase::kToHalf;
                boot_phase_started_ms_ = now;
            }
            break;
        case BootPhase::kToHalf:
            if (now - boot_phase_started_ms_ >= kBootToHalfMs) {
                boot_phase_ = BootPhase::kHalfBlink;
                boot_blink_count_ = 0;
                last_boot_blink_ms_ = now;
            }
            break;
        case BootPhase::kHalfBlink:
            if (boot_blink_count_ >= kBootBlinkCount && blink_state_ == BlinkState::kIdle) {
                boot_phase_ = BootPhase::kToFull;
                boot_phase_started_ms_ = now;
            }
            break;
        case BootPhase::kToFull:
            if (now - boot_phase_started_ms_ >= kBootToFullMs) {
                boot_phase_ = BootPhase::kIdle;
                next_blink_ms_ = now + RandomBlinkInterval();
                next_gaze_ms_ = now + Random(kGazeIntervalMinMs, kGazeIntervalMaxMs);
                next_half_blink_ms_ =
                    now + Random(kHalfBlinkIntervalMinMs, kHalfBlinkIntervalMaxMs);
                next_drift_ms_ = now + kDriftIntervalMs;
            }
            break;
        case BootPhase::kIdle:
            break;
    }
}

void RobotEyeDisplay::UpdateBlink(uint32_t now) {
    if (boot_phase_ == BootPhase::kToHalf || boot_phase_ == BootPhase::kToFull) {
        return;
    }
    switch (blink_state_) {
        case BlinkState::kIdle:
            if (boot_phase_ == BootPhase::kHalfBlink) {
                if (boot_blink_count_ < kBootBlinkCount &&
                    now - last_boot_blink_ms_ >= kBootHalfBlinkGapMs) {
                    StartBlink(now);
                    ++boot_blink_count_;
                }
            } else if (boot_phase_ == BootPhase::kIdle && DeadlineReached(now, next_blink_ms_)) {
                StartBlink(now);
                next_blink_ms_ = now + RandomBlinkInterval();
            }
            break;
        case BlinkState::kClosing:
            if (now - blink_started_ms_ >= kBlinkCloseMs) {
                blink_state_ = BlinkState::kHold;
                blink_started_ms_ = now;
                blink_hold_ms_ = Random(kBlinkHoldMinMs, kBlinkHoldMaxMs + 1);
            }
            break;
        case BlinkState::kHold:
            if (now - blink_started_ms_ >= blink_hold_ms_) {
                blink_state_ = BlinkState::kOpening;
                blink_started_ms_ = now;
            }
            break;
        case BlinkState::kOpening:
            if (now - blink_started_ms_ >= kBlinkOpenMs) {
                blink_state_ = BlinkState::kIdle;
                if (transition_state_ == TransitionState::kBlinkSwitch) {
                    current_expression_ = transition_to_;
                    transition_state_ = TransitionState::kIdle;
                }
                if (boot_phase_ == BootPhase::kHalfBlink) {
                    last_boot_blink_ms_ = now;
                }
            }
            break;
    }
}

void RobotEyeDisplay::UpdateHalfBlink(uint32_t now) {
    if (boot_phase_ != BootPhase::kIdle || blink_state_ != BlinkState::kIdle ||
        transition_state_ != TransitionState::kIdle) {
        return;
    }
    switch (half_blink_state_) {
        case HalfBlinkState::kIdle:
            if (DeadlineReached(now, next_half_blink_ms_)) {
                if (Random(0, 100) < 30) {
                    half_blink_state_ = HalfBlinkState::kShrinking;
                    half_blink_started_ms_ = now;
                }
                next_half_blink_ms_ =
                    now + Random(kHalfBlinkIntervalMinMs, kHalfBlinkIntervalMaxMs);
            }
            break;
        case HalfBlinkState::kShrinking:
            if (now - half_blink_started_ms_ >= kHalfBlinkPhaseMs) {
                half_blink_state_ = HalfBlinkState::kExpanding;
                half_blink_started_ms_ = now;
            }
            break;
        case HalfBlinkState::kExpanding:
            if (now - half_blink_started_ms_ >= kHalfBlinkPhaseMs) {
                half_blink_state_ = HalfBlinkState::kIdle;
            }
            break;
    }
}

void RobotEyeDisplay::UpdateGaze(uint32_t now) {
    if (speaking_ || boot_phase_ != BootPhase::kIdle ||
        current_expression_ != Expression::kNormal || transition_state_ != TransitionState::kIdle) {
        gaze_state_ = GazeState::kIdle;
        return;
    }
    switch (gaze_state_) {
        case GazeState::kIdle:
            if (blink_state_ != BlinkState::kIdle || !DeadlineReached(now, next_gaze_ms_)) {
                break;
            }
            if (Random(0, 100) < 30) {
                static constexpr Expression directions[] = {
                    Expression::kLookLeft,
                    Expression::kLookRight,
                    Expression::kLookUp,
                    Expression::kLookDown,
                };
                gaze_expression_ = directions[Random(0, 4)];
                gaze_state_ = GazeState::kToDirection;
                gaze_started_ms_ = now;
            }
            next_gaze_ms_ = now + Random(kGazeIntervalMinMs, kGazeIntervalMaxMs);
            break;
        case GazeState::kToDirection:
            if (now - gaze_started_ms_ >= kGazeTransitionMs) {
                gaze_state_ = GazeState::kHolding;
                gaze_hold_until_ms_ = now + Random(kGazeHoldMinMs, kGazeHoldMaxMs + 1);
            }
            break;
        case GazeState::kHolding:
            if (DeadlineReached(now, gaze_hold_until_ms_)) {
                gaze_state_ = GazeState::kToCenter;
                gaze_started_ms_ = now;
            }
            break;
        case GazeState::kToCenter:
            if (now - gaze_started_ms_ >= kGazeTransitionMs) {
                gaze_state_ = GazeState::kIdle;
            }
            break;
    }
}

void RobotEyeDisplay::UpdateDrift(uint32_t now) {
    if (boot_phase_ != BootPhase::kIdle || current_expression_ != Expression::kNormal) {
        return;
    }
    if (DeadlineReached(now, next_drift_ms_)) {
        drift_target_x_ = static_cast<float>(static_cast<int>(Random(0, 9)) - 4) * geometry_scale_;
        drift_target_y_ = static_cast<float>(static_cast<int>(Random(0, 9)) - 4) * geometry_scale_;
        next_drift_ms_ = now + kDriftIntervalMs;
    }
    drift_x_ += (drift_target_x_ - drift_x_) * 0.40f;
    drift_y_ += (drift_target_y_ - drift_y_) * 0.40f;
}

void RobotEyeDisplay::UpdateTransition(uint32_t now) {
    if (transition_state_ == TransitionState::kMorph &&
        now - transition_started_ms_ >= kMorphDurationMs) {
        current_expression_ = transition_to_;
        transition_state_ = TransitionState::kIdle;
    }
}

void RobotEyeDisplay::AnimationTimerCallback(lv_timer_t* timer) {
    auto* display = static_cast<RobotEyeDisplay*>(lv_timer_get_user_data(timer));
    if (display != nullptr) {
        display->UpdateAnimation();
    }
}

void RobotEyeDisplay::UpdateAnimation() {
    if (canvas_pixels_ == nullptr) {
        return;
    }
    const uint32_t now = NowMs();
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
    if (now - last_drive_refresh_ms_ >= 200) {
        last_drive_refresh_ms_ = now;
        const bool driving = ReadDriveStatus().active;
        if (driving) {
            if (active_screen_ != Screen::kDrive)
                ShowScreen(Screen::kDrive, 1000);
            else
                screen_until_ms_ = now + 1000;
        } else if (active_screen_ == Screen::kDrive) {
            ShowEyesScreen();
        }
    }
#endif
    if (active_screen_ != Screen::kEyes) {
        if (screen_until_ms_ != 0 && DeadlineReached(now, screen_until_ms_)) {
            ShowEyesScreen();
        } else {
            RenderWeatherScene(now);
            RefreshInfoScreen(now);
            return;
        }
    }
    if (happy_until_ms_ != 0 && DeadlineReached(now, happy_until_ms_)) {
        happy_until_ms_ = 0;
        SetRobotEyeExpression(Expression::kNormal);
        NotifyEmotionMotion();
    }

    UpdateBoot(now);
    UpdateTransition(now);
    UpdateGaze(now);
    UpdateDrift(now);
    UpdateHalfBlink(now);
    UpdateBlink(now);

    float excited_amount = current_expression_ == Expression::kExcited ? 1.0f : 0.0f;
    if (transition_state_ == TransitionState::kMorph) {
        const float amount =
            EaseInOut(Clamp01(static_cast<float>(now - transition_started_ms_) / kMorphDurationMs));
        if (transition_to_ == Expression::kExcited) {
            excited_amount = amount;
        } else if (transition_from_ == Expression::kExcited) {
            excited_amount = 1.0f - amount;
        }
    }

    const FaceParams face = ComputeFrame(now);
    ClearCanvas();
    DrawFace(face);
    if (excited_amount > 0.01f) {
        DrawExcitedMask(face, excited_amount);
    }
    PresentCanvas();
}
