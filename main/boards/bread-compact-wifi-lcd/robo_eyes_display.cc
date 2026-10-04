#include "robo_eyes_display.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include <esp_log.h>
#include <esp_random.h>

#include "assets/lang_config.h"
#include "display/lvgl_display/lvgl_theme.h"

#define TAG "RoboEyesDisplay"

namespace {
constexpr int kReferenceHeight = 80;
constexpr int kMaxFullRefreshPixels = 160 * kReferenceHeight;
// Eye dimensions enlarged twice by 20%, rounded to whole pixels each time.
constexpr int kEyeWidth = 41;
constexpr int kEyeHeight = 66;
constexpr int kEyeGap = 18;
constexpr int kClosedEyeHeight = 6;
constexpr int kEyeTimerPeriodMs = 30;
constexpr int kScreenMargin = 4;

int ApproachTarget(int position, int target) {
    const int distance = target - position;
    const int step = std::max(2, (std::abs(distance) + 3) / 4);
    return position + std::clamp(distance, -step, step);
}

bool ContainsPhrase(std::string_view text, std::string_view phrase) {
    // Preserve UTF-8 bytes; fold ASCII letters only. Accented initials are listed explicitly.
    return std::search(text.begin(), text.end(), phrase.begin(), phrase.end(), [](char a, char b) {
               auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; };
               return lower(a) == lower(b);
           }) != text.end();
}
}  // namespace

RoboEyesDisplay::RoboEyesDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                 int width, int height, int offset_x, int offset_y, bool mirror_x,
                                 bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    width * height <= kMaxFullRefreshPixels) {}

RoboEyesDisplay::~RoboEyesDisplay() {
    if (eye_timer_ != nullptr) {
        lv_timer_delete(eye_timer_);
    }
}

void RoboEyesDisplay::SetupUI() {
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }

    SpiLcdDisplay::SetupUI();

    auto* dark_theme = LvglThemeManager::GetInstance().GetTheme("dark");
    if (dark_theme != nullptr) {
        SetTheme(dark_theme);
    }

    DisplayLockGuard lock(this);
    if (!lock || container_ == nullptr) {
        ESP_LOGE(TAG, "Cannot create eyes before the LCD UI is ready");
        return;
    }

    if (emoji_box_ != nullptr) {
        lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    }
    if (preview_image_ != nullptr) {
        lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
    }
    if (top_bar_ != nullptr) {
        lv_obj_add_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (status_bar_ != nullptr) {
        lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (content_ != nullptr) {
        lv_obj_add_flag(content_, LV_OBJ_FLAG_HIDDEN);
    }
    if (bottom_bar_ != nullptr) {
        lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (low_battery_popup_ != nullptr) {
        lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
    }

    eyes_layer_ = lv_obj_create(container_);
    // Keep panel coordinates independent of the inherited chat layout and scrolling.
    lv_obj_add_flag(eyes_layer_, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_flag(eyes_layer_, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(eyes_layer_, 0, 0);
    lv_obj_set_size(eyes_layer_, width_, height_);
    lv_obj_set_style_radius(eyes_layer_, 0, 0);
    lv_obj_set_style_bg_color(eyes_layer_, lv_color_hex(0x050A12), 0);
    lv_obj_set_style_bg_opa(eyes_layer_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eyes_layer_, 0, 0);
    lv_obj_set_style_pad_all(eyes_layer_, 0, 0);
    lv_obj_set_scrollbar_mode(eyes_layer_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(eyes_layer_, LV_OBJ_FLAG_SCROLLABLE);

    left_eye_ = lv_obj_create(eyes_layer_);
    right_eye_ = lv_obj_create(eyes_layer_);

    for (auto* eye : {left_eye_, right_eye_}) {
        lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_shadow_width(eye, 0, 0);
        lv_obj_set_style_shadow_opa(eye, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(eye, LV_OBJ_FLAG_SCROLLABLE);
    }

    left_brow_ = lv_line_create(eyes_layer_);
    right_brow_ = lv_line_create(eyes_layer_);
    for (auto* brow : {left_brow_, right_brow_}) {
        lv_obj_set_style_line_width(brow, 4, 0);
        lv_obj_set_style_line_rounded(brow, true, 0);
        lv_obj_set_style_line_color(brow, lv_color_hex(0x00E5F5), 0);
        lv_obj_add_flag(brow, LV_OBJ_FLAG_HIDDEN);
    }

    ApplyExpressionColor();
    UpdateEyes();
    eye_timer_ = lv_timer_create(EyeTimerCallback, kEyeTimerPeriodMs, this);
    ESP_LOGI(TAG, "Robo eyes ready");
}

bool RoboEyesDisplay::ParseEmotion(const char* emotion, Expression& expression) {
    if (emotion == nullptr) {
        return false;
    }
    static constexpr struct {
        std::string_view name;
        Expression expression;
    } emotions[] = {
        {"neutral", Expression::kNeutral},
        {"idle", Expression::kNeutral},
        {"happy", Expression::kHappy},
        {"laughing", Expression::kHappy},
        {"laugh", Expression::kHappy},
        {"funny", Expression::kHappy},
        {"loving", Expression::kHappy},
        {"embarrassed", Expression::kHappy},
        {"confident", Expression::kHappy},
        {"delicious", Expression::kHappy},
        {"silly", Expression::kWinking},
        {"winking", Expression::kWinking},
        {"cool", Expression::kWinking},
        {"kissy", Expression::kHappy},
        {"sad", Expression::kSad},
        {"crying", Expression::kSad},
        {"angry", Expression::kAngry},
        {"mad", Expression::kAngry},
        {"surprised", Expression::kSurprised},
        {"shocked", Expression::kSurprised},
        {"thinking", Expression::kThinking},
        {"confused", Expression::kThinking},
        {"sleepy", Expression::kSleepy},
        {"sleep", Expression::kSleepy},
        {"tired", Expression::kSleepy},
        {"relaxed", Expression::kSleepy},
    };
    for (const auto& entry : emotions) {
        if (entry.name == emotion) {
            expression = entry.expression;
            return true;
        }
    }
    return false;
}

void RoboEyesDisplay::SetStatus(const char* status) {
    if (status == nullptr) {
        return;
    }
    SpiLcdDisplay::SetStatus(status);
    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }
    if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
        speaking_ = true;
        status_neutral_pending_ = false;
        happy_ticks_remaining_ = 0;
        CenterGaze();
        UpdateEyes();
        // Emotion/sentence events can precede the scheduled speaking-state update.
        if (server_emotion_received_ || response_sentence_received_) {
            NotifyEmotionMotion();
        } else if (emotion_motion_callback_ != nullptr) {
            emotion_motion_callback_("neutral", true);
        }
    } else if (strcmp(status, Lang::Strings::LISTENING) == 0 ||
               strcmp(status, Lang::Strings::STANDBY) == 0 ||
               strcmp(status, Lang::Strings::CONNECTING) == 0) {
        speaking_ = false;
        server_emotion_received_ = false;
        response_sentence_received_ = false;
        // Application follows these status updates with SetEmotion("neutral").
        status_neutral_pending_ = true;
        if (emotion_motion_callback_ != nullptr) {
            emotion_motion_callback_("neutral", false);
        }
    }
}

void RoboEyesDisplay::SetEmotionMotionCallback(EmotionMotionCallback callback) {
    DisplayLockGuard lock(this);
    if (lock) {
        emotion_motion_callback_ = callback;
    }
}

void RoboEyesDisplay::NotifyEmotionMotion() {
    if (emotion_motion_callback_ == nullptr) {
        return;
    }
    const char* emotion = "neutral";
    switch (expression_) {
        case Expression::kHappy:
            emotion = "happy";
            break;
        case Expression::kSad:
            emotion = "sad";
            break;
        case Expression::kAngry:
            emotion = "angry";
            break;
        case Expression::kSurprised:
            emotion = "surprised";
            break;
        case Expression::kThinking:
            emotion = "thinking";
            break;
        case Expression::kWinking:
            emotion = "winking";
            break;
        case Expression::kSleepy:
            emotion = "sleepy";
            break;
        default:
            break;
    }
    emotion_motion_callback_(emotion, speaking_ && happy_ticks_remaining_ == 0);
}

void RoboEyesDisplay::SetEmotion(const char* emotion) {
    Expression next_expression;
    if (!ParseEmotion(emotion, next_expression)) {
        return;  // Unknown tags must not erase a valid expression.
    }
    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }
    const bool status_reset = status_neutral_pending_ && next_expression == Expression::kNeutral;
    status_neutral_pending_ = false;
    // Only startup neutral resets may leave the local welcome animation running.
    if (happy_ticks_remaining_ > 0 && !speaking_ && next_expression == Expression::kNeutral) {
        return;
    }
    happy_ticks_remaining_ = 0;
    server_emotion_received_ = !status_reset;
    ApplyExpression(next_expression);
    NotifyEmotionMotion();
}

void RoboEyesDisplay::ApplyExpression(Expression expression) {
    if (expression_ != expression) {
        expression_ = expression;
        blink_frame_ = 0;
        blink_countdown_ = 65 + static_cast<int>(esp_random() % 50);
    }
    if (!CanMoveGaze()) {
        CenterGaze();
    }
    if (left_eye_ != nullptr) {
        UpdateEyes();
    }
}

bool RoboEyesDisplay::CanMoveGaze() const {
    return !speaking_ && expression_ == Expression::kNeutral && happy_ticks_remaining_ == 0;
}

void RoboEyesDisplay::CenterGaze() {
    // Cancel any in-flight movement as soon as speech or an expression begins.
    gaze_x_ = 0;
    gaze_y_ = 0;
    gaze_target_x_ = 0;
    gaze_target_y_ = 0;
    gaze_hold_ticks_ = 25;
}

void RoboEyesDisplay::ShowHappyFor(uint32_t duration_ms) {
    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }

    if (speaking_) {
        return;
    }
    ApplyExpression(Expression::kHappy);
    happy_ticks_remaining_ =
        std::max<uint32_t>(1, (duration_ms + kEyeTimerPeriodMs - 1) / kEyeTimerPeriodMs);
    // Keep the greeting expression centered and still.  Normal gaze motion
    // resumes automatically once the happy timer expires.
    CenterGaze();
    if (left_eye_ != nullptr) {
        UpdateEyes();
    }
}

RoboEyesDisplay::Expression RoboEyesDisplay::InferSentenceExpression(const char* content) {
    // Conservative fallback only. The server's emotion tag has priority over text clues.
    const std::string_view text(content);
    for (auto phrase : {"xin lỗi", "rất tiếc", "chia buồn", "i'm sorry", "my condolences"}) {
        if (ContainsPhrase(text, phrase)) {
            return Expression::kSad;
        }
    }
    for (auto phrase : {"chúc mừng", "rất vui được", "congratulations", "happy birthday"}) {
        if (ContainsPhrase(text, phrase)) {
            return Expression::kHappy;
        }
    }
    for (auto phrase : {"thật bất ngờ", "thật ngạc nhiên", "what a surprise"}) {
        if (ContainsPhrase(text, phrase)) {
            return Expression::kSurprised;
        }
    }
    for (auto phrase : {"chúc ngủ ngon", "chúc bạn ngủ ngon", "good night"}) {
        if (ContainsPhrase(text, phrase)) {
            return Expression::kSleepy;
        }
    }
    for (auto phrase : {"để mình nghĩ", "Để mình nghĩ", "để tôi nghĩ", "Để tôi nghĩ",
                        "let me think", "let me check"}) {
        if (ContainsPhrase(text, phrase)) {
            return Expression::kThinking;
        }
    }
    return Expression::kNeutral;
}

void RoboEyesDisplay::SetChatMessage(const char* role, const char* content) {
    if (role == nullptr || content == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }
    if (strcmp(role, "user") == 0 && content[0] != '\0') {
        happy_ticks_remaining_ = 0;
        server_emotion_received_ = false;
        response_sentence_received_ = false;
        status_neutral_pending_ = false;
        CenterGaze();
        ApplyExpression(Expression::kThinking);
        if (emotion_motion_callback_ != nullptr) {
            emotion_motion_callback_("neutral", false);
        }
    } else if (strcmp(role, "assistant") == 0) {
        response_sentence_received_ = content[0] != '\0';
        happy_ticks_remaining_ = 0;
        CenterGaze();
        if (content[0] == '\0') {
            server_emotion_received_ = false;
            ApplyExpression(Expression::kNeutral);
        } else if (!server_emotion_received_) {
            // Called by the existing TTS sentence_start/subtitle path, not by incoming STT text.
            ApplyExpression(InferSentenceExpression(content));
        }
        NotifyEmotionMotion();
    }
}

void RoboEyesDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }
    server_emotion_received_ = false;
    response_sentence_received_ = false;
    if (emotion_motion_callback_ != nullptr) {
        emotion_motion_callback_("neutral", false);
    }
    if (happy_ticks_remaining_ == 0) {
        ApplyExpression(Expression::kNeutral);
    }
}

void RoboEyesDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) { (void)image; }

void RoboEyesDisplay::EyeTimerCallback(lv_timer_t* timer) {
    auto* display = static_cast<RoboEyesDisplay*>(lv_timer_get_user_data(timer));
    if (display == nullptr || display->left_eye_ == nullptr) {
        return;
    }
    const auto previous_expression = display->expression_;
    const int previous_blink = display->blink_frame_;
    const int previous_x = display->gaze_x_;
    const int previous_y = display->gaze_y_;

    if (display->happy_ticks_remaining_ > 0 && --display->happy_ticks_remaining_ == 0) {
        display->expression_ = Expression::kNeutral;
    }

    if (display->expression_ == Expression::kSleepy) {
        display->blink_frame_ = 3;
    } else if (display->blink_frame_ > 0) {
        if (++display->blink_frame_ > 6) {
            display->blink_frame_ = 0;
            display->blink_countdown_ = 65 + static_cast<int>(esp_random() % 50);
        }
    } else if (--display->blink_countdown_ <= 0) {
        display->blink_frame_ = 1;
    }

    display->ClampGaze();
    if (!display->CanMoveGaze()) {
        display->CenterGaze();
    } else if (display->gaze_x_ == display->gaze_target_x_ &&
               display->gaze_y_ == display->gaze_target_y_ && --display->gaze_hold_ticks_ <= 0) {
        static constexpr int kGazeTargets[][2] = {
            {0, 0}, {1, 0}, {-1, 0}, {0, -1}, {0, 1}, {1, -1}, {-1, -1}, {1, 1}, {-1, 1},
        };
        const auto& target =
            kGazeTargets[esp_random() % (sizeof(kGazeTargets) / sizeof(kGazeTargets[0]))];
        const auto limits = display->GetGazeLimits();
        display->gaze_target_x_ = target[0] * limits.x;
        display->gaze_target_y_ = target[1] * limits.y;
        display->gaze_hold_ticks_ = 18 + static_cast<int>(esp_random() % 23);
    }

    display->gaze_x_ = ApproachTarget(display->gaze_x_, display->gaze_target_x_);
    display->gaze_y_ = ApproachTarget(display->gaze_y_, display->gaze_target_y_);

    if (display->expression_ != previous_expression || display->blink_frame_ != previous_blink ||
        display->gaze_x_ != previous_x || display->gaze_y_ != previous_y) {
        display->UpdateEyes();
    }
}

void RoboEyesDisplay::ApplyExpressionColor() {
    const lv_color_t eye_color = lv_color_hex(0x00E5F5);

    for (auto* eye : {left_eye_, right_eye_}) {
        lv_obj_set_style_bg_color(eye, eye_color, 0);
        lv_obj_set_style_shadow_color(eye, eye_color, 0);
    }
}

RoboEyesDisplay::EyeShape RoboEyesDisplay::GetEyeShape() const {
    switch (expression_) {
        case Expression::kHappy:
            return {55, 20, 20, 0};
        case Expression::kAngry:
            return {kEyeWidth, 30, 30, 14};
        case Expression::kSad:
            return {kEyeWidth, 38, 38, 14};
        case Expression::kSurprised:
            return {50, 70, 70, 0};
        case Expression::kThinking:
            return {kEyeWidth, 46, 30, 14};
        case Expression::kWinking:
            return {kEyeWidth, kClosedEyeHeight, kEyeHeight, 0};
        case Expression::kSleepy:
            return {kEyeWidth, kClosedEyeHeight, kClosedEyeHeight, 0};
        default:
            return {kEyeWidth, kEyeHeight, kEyeHeight, 0};
    }
}

RoboEyesDisplay::GazeLimits RoboEyesDisplay::GetGazeLimits() const {
    const auto shape = GetEyeShape();
    const int max_x = std::max(0, (width_ - 2 * shape.width - kEyeGap) / 2 - kScreenMargin);
    const int max_y = std::max(
        0, (height_ - std::max(shape.left_height, shape.right_height) - shape.brow_height) / 2 -
               kScreenMargin);
    return {max_x, max_y};
}

void RoboEyesDisplay::ClampGaze() {
    const auto limits = GetGazeLimits();
    gaze_x_ = std::clamp(gaze_x_, -limits.x, limits.x);
    gaze_y_ = std::clamp(gaze_y_, -limits.y, limits.y);
    gaze_target_x_ = std::clamp(gaze_target_x_, -limits.x, limits.x);
    gaze_target_y_ = std::clamp(gaze_target_y_, -limits.y, limits.y);
}

void RoboEyesDisplay::UpdateEyes() {
    if (left_eye_ == nullptr || right_eye_ == nullptr) {
        return;
    }
    ClampGaze();
    const auto shape = GetEyeShape();
    const int open_height = std::max(shape.left_height, shape.right_height);
    const int left_x = (width_ - 2 * shape.width - kEyeGap) / 2 + gaze_x_;
    const int right_x = left_x + shape.width + kEyeGap;
    const int top = (height_ - open_height - shape.brow_height) / 2 + gaze_y_;
    const int closing = blink_frame_ <= 3 ? blink_frame_ : 6 - blink_frame_;
    auto place_eye = [&](lv_obj_t* eye, int x, int height) {
        const int animated_height = std::max(kClosedEyeHeight, height * (3 - closing) / 3);
        lv_obj_set_size(eye, shape.width, animated_height);
        lv_obj_set_pos(eye, x, top + shape.brow_height + (open_height - animated_height) / 2);
    };
    place_eye(left_eye_, left_x, shape.left_height);
    place_eye(right_eye_, right_x, shape.right_height);

    if (shape.brow_height == 0) {
        lv_obj_add_flag(left_brow_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_brow_, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Endpoints sit inside the screen margin, including the rounded line caps.
        const int slope = expression_ == Expression::kAngry ? 7 : -7;
        lv_point_precise_set(&left_brow_points_[0], 2, slope > 0 ? 2 : 9);
        lv_point_precise_set(&left_brow_points_[1], shape.width - 3, slope > 0 ? 9 : 2);
        lv_point_precise_set(&right_brow_points_[0], 2, slope > 0 ? 9 : 2);
        lv_point_precise_set(&right_brow_points_[1], shape.width - 3, slope > 0 ? 2 : 9);
        if (expression_ == Expression::kThinking) {
            right_brow_points_[0].y = 7;
            right_brow_points_[1].y = 7;
        }
        lv_line_set_points(left_brow_, left_brow_points_, 2);
        lv_line_set_points(right_brow_, right_brow_points_, 2);
        lv_obj_set_pos(left_brow_, left_x, top);
        lv_obj_set_pos(right_brow_, right_x, top);
        lv_obj_remove_flag(left_brow_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(right_brow_, LV_OBJ_FLAG_HIDDEN);
    }
}
