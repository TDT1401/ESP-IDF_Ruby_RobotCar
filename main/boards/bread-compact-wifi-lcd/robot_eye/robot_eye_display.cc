#include "robot_eye_display.h"

#include <algorithm>
#include <cstring>
#include <string_view>

#include <esp_log.h>

#include "assets/lang_config.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "robot_eye_constants.h"

#define TAG "RobotEyeDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(font_material_symbols_30_4);

using namespace robot_eye_detail;

namespace {
bool ContainsPhrase(std::string_view text, std::string_view phrase) {
    return std::search(text.begin(), text.end(), phrase.begin(), phrase.end(), [](char a, char b) {
               const auto lower = [](unsigned char character) {
                   return character >= 'A' && character <= 'Z' ? character + 32 : character;
               };
               return lower(a) == lower(b);
           }) != text.end();
}
}  // namespace

RobotEyeDisplay::RobotEyeDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                 int width, int height, int offset_x, int offset_y, bool mirror_x,
                                 bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    width * height <= kMaxFullRefreshPixels) {
    // Keep enough margin for RobotEye's LOOK_LEFT/LOOK_RIGHT geometry.
    geometry_scale_ = width >= 200 && height >= 200 ? 2 : 1;
    const int total_width = Scale(kReferenceEyeWidth * 2 + kReferenceEyeGap);
    base_left_x_ = static_cast<int16_t>((width_ - total_width) / 2);
    base_right_x_ =
        static_cast<int16_t>(base_left_x_ + Scale(kReferenceEyeWidth + kReferenceEyeGap));
    base_y_ = static_cast<int16_t>((height_ - Scale(kReferenceEyeHeight)) / 2);
}

RobotEyeDisplay::~RobotEyeDisplay() {
    if (animation_timer_ != nullptr) {
        lv_timer_delete(animation_timer_);
        animation_timer_ = nullptr;
    }
    if (info_layer_ != nullptr) {
        lv_obj_delete(info_layer_);
        info_layer_ = nullptr;
        weather_canvas_ = nullptr;
    }
    if (weather_draw_buffer_ != nullptr) {
        lv_draw_buf_destroy(weather_draw_buffer_);
        weather_draw_buffer_ = nullptr;
    }
    if (eyes_canvas_ != nullptr) {
        lv_obj_delete(eyes_canvas_);
        eyes_canvas_ = nullptr;
    }
    if (canvas_draw_buffer_ != nullptr) {
        lv_draw_buf_destroy(canvas_draw_buffer_);
        canvas_draw_buffer_ = nullptr;
    }
}

void RobotEyeDisplay::SetupUI() {
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
        ESP_LOGE(TAG, "Cannot create RobotEye canvas before the LCD UI is ready");
        return;
    }

    for (auto* object : {emoji_box_, preview_image_, top_bar_, status_bar_, content_, bottom_bar_,
                         low_battery_popup_}) {
        if (object != nullptr) {
            lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
        }
    }

    eyes_canvas_ = lv_canvas_create(container_);
    canvas_draw_buffer_ = lv_draw_buf_create(width_, height_, LV_COLOR_FORMAT_I1, LV_STRIDE_AUTO);
    if (eyes_canvas_ == nullptr || canvas_draw_buffer_ == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate the RobotEye canvas");
        if (eyes_canvas_ != nullptr) {
            lv_obj_delete(eyes_canvas_);
            eyes_canvas_ = nullptr;
        }
        if (canvas_draw_buffer_ != nullptr) {
            lv_draw_buf_destroy(canvas_draw_buffer_);
            canvas_draw_buffer_ = nullptr;
        }
        return;
    }

    lv_canvas_set_draw_buf(eyes_canvas_, canvas_draw_buffer_);
    lv_canvas_set_palette(eyes_canvas_, 0, lv_color_to_32(lv_color_hex(0x050A12), LV_OPA_COVER));
    lv_canvas_set_palette(eyes_canvas_, 1, lv_color_to_32(lv_color_hex(0x00E5F5), LV_OPA_COVER));
    lv_obj_add_flag(eyes_canvas_, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_flag(eyes_canvas_, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(eyes_canvas_, 0, 0);
    lv_obj_set_size(eyes_canvas_, width_, height_);

    info_layer_ = lv_obj_create(container_);
    lv_obj_add_flag(info_layer_, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_flag(info_layer_, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(info_layer_, 0, 0);
    lv_obj_set_size(info_layer_, width_, height_);
    lv_obj_set_style_radius(info_layer_, 0, 0);
    lv_obj_set_style_bg_color(info_layer_, lv_color_hex(0x050A12), 0);
    lv_obj_set_style_bg_opa(info_layer_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(info_layer_, 0, 0);
    lv_obj_set_style_pad_all(info_layer_, 0, 0);
    lv_obj_set_scrollbar_mode(info_layer_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(info_layer_, LV_OBJ_FLAG_SCROLLABLE);

    info_icon_label_ = lv_label_create(info_layer_);
    lv_obj_set_style_text_font(info_icon_label_, &font_material_symbols_30_4, 0);
    lv_obj_set_style_text_color(info_icon_label_, lv_color_hex(0x00E5F5), 0);

    info_title_label_ = lv_label_create(info_layer_);
    info_value_label_ = lv_label_create(info_layer_);
    info_detail_label_ = lv_label_create(info_layer_);
    for (auto* label : {info_title_label_, info_value_label_, info_detail_label_}) {
        lv_obj_set_style_text_font(label, &BUILTIN_TEXT_FONT, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
    }
    lv_obj_set_style_text_color(info_title_label_, lv_color_hex(0x00E5F5), 0);
    lv_obj_set_style_text_color(info_detail_label_, lv_color_hex(0x8CA6B8), 0);
    lv_label_set_long_mode(info_detail_label_, LV_LABEL_LONG_WRAP);

    weather_canvas_width_ = height_ <= 100 ? 52 : std::min(112, width_ - 24);
    weather_canvas_height_ = height_ <= 100 ? 52 : 82;
    weather_canvas_ = lv_canvas_create(info_layer_);
    weather_draw_buffer_ = lv_draw_buf_create(weather_canvas_width_, weather_canvas_height_,
                                              LV_COLOR_FORMAT_I1, LV_STRIDE_AUTO);
    if (weather_canvas_ != nullptr && weather_draw_buffer_ != nullptr) {
        lv_canvas_set_draw_buf(weather_canvas_, weather_draw_buffer_);
        lv_canvas_set_palette(weather_canvas_, 0,
                              lv_color_to_32(lv_color_hex(0x050A12), LV_OPA_COVER));
        lv_canvas_set_palette(weather_canvas_, 1,
                              lv_color_to_32(lv_color_hex(0x00E5F5), LV_OPA_COVER));
        weather_pixels_ = static_cast<uint8_t*>(lv_draw_buf_goto_xy(weather_draw_buffer_, 0, 0));
        weather_stride_ = weather_draw_buffer_->header.stride;
    }
    if (weather_canvas_ == nullptr || weather_draw_buffer_ == nullptr ||
        weather_pixels_ == nullptr || weather_stride_ == 0) {
        ESP_LOGW(TAG, "Weather canvas allocation failed; using the thermometer font icon");
        if (weather_canvas_ != nullptr) {
            lv_obj_delete(weather_canvas_);
            weather_canvas_ = nullptr;
        }
        if (weather_draw_buffer_ != nullptr) {
            lv_draw_buf_destroy(weather_draw_buffer_);
            weather_draw_buffer_ = nullptr;
        }
        weather_pixels_ = nullptr;
        weather_stride_ = 0;
    } else {
        lv_obj_add_flag(weather_canvas_, LV_OBJ_FLAG_HIDDEN);
    }

    if (height_ <= 100) {
        const int text_width = std::max(80, width_ - 58);
        lv_label_set_long_mode(info_detail_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(info_icon_label_, LV_ALIGN_LEFT_MID, 12, 0);
        if (weather_canvas_ != nullptr) {
            lv_obj_align(weather_canvas_, LV_ALIGN_LEFT_MID, 2, 0);
        }
        lv_obj_set_width(info_title_label_, text_width);
        lv_obj_align(info_title_label_, LV_ALIGN_TOP_RIGHT, -2, 2);
        lv_obj_set_width(info_value_label_, text_width);
        lv_obj_align(info_value_label_, LV_ALIGN_RIGHT_MID, -2, -2);
        lv_obj_set_width(info_detail_label_, text_width);
        lv_obj_align(info_detail_label_, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
    } else {
        const int text_width = width_ - 16;
        lv_obj_align(info_icon_label_, LV_ALIGN_TOP_MID, 0, 48);
        if (weather_canvas_ != nullptr) {
            lv_obj_align(weather_canvas_, LV_ALIGN_TOP_MID, 0, 38);
        }
        lv_obj_set_width(info_title_label_, text_width);
        lv_obj_align(info_title_label_, LV_ALIGN_TOP_MID, 0, 16);
        lv_obj_set_width(info_value_label_, text_width);
        lv_obj_align(info_value_label_, LV_ALIGN_CENTER, 0, 22);
        lv_obj_set_width(info_detail_label_, text_width);
        lv_obj_align(info_detail_label_, LV_ALIGN_BOTTOM_MID, 0, -28);
    }
    lv_obj_add_flag(info_layer_, LV_OBJ_FLAG_HIDDEN);

    canvas_pixels_ = static_cast<uint8_t*>(lv_draw_buf_goto_xy(canvas_draw_buffer_, 0, 0));
    canvas_stride_ = canvas_draw_buffer_->header.stride;
    if (canvas_pixels_ == nullptr || canvas_stride_ == 0) {
        ESP_LOGE(TAG, "RobotEye canvas buffer is invalid");
        return;
    }

    const uint32_t now = NowMs();
    boot_phase_started_ms_ = now;
    blink_started_ms_ = now;
    ClearCanvas();
    DrawFace(ComputeFrame(now));
    PresentCanvas();
    animation_timer_ = lv_timer_create(AnimationTimerCallback, kAnimationPeriodMs, this);
    ESP_LOGI(TAG, "RobotEye 1.0.0 LVGL port ready (%dx%d, geometry scale %d)", width_, height_,
             geometry_scale_);
}

bool RobotEyeDisplay::ParseEmotion(const char* emotion, Expression& expression) {
    if (emotion == nullptr) {
        return false;
    }
    static constexpr struct {
        std::string_view name;
        Expression expression;
    } emotions[] = {
        {"neutral", Expression::kNormal},
        {"idle", Expression::kNormal},
        {"happy", Expression::kHappy},
        {"laughing", Expression::kExcited},
        {"laugh", Expression::kExcited},
        {"funny", Expression::kExcited},
        {"loving", Expression::kHappy},
        {"embarrassed", Expression::kWorried},
        {"confident", Expression::kFocused},
        {"delicious", Expression::kHappy},
        {"silly", Expression::kWinkLeft},
        {"winking", Expression::kWinkLeft},
        {"cool", Expression::kFocused},
        {"kissy", Expression::kHappy},
        {"sad", Expression::kSad},
        {"crying", Expression::kDespair},
        {"angry", Expression::kAngry},
        {"mad", Expression::kFurious},
        {"surprised", Expression::kSurprised},
        {"shocked", Expression::kScared},
        {"thinking", Expression::kFocused},
        {"confused", Expression::kDisoriented},
        {"sleepy", Expression::kSleepy},
        {"sleep", Expression::kBlinkDown},
        {"tired", Expression::kBored},
        {"relaxed", Expression::kSleepy},
        {"worried", Expression::kWorried},
        {"scared", Expression::kScared},
    };
    for (const auto& entry : emotions) {
        if (entry.name == emotion) {
            expression = entry.expression;
            return true;
        }
    }
    return false;
}

void RobotEyeDisplay::SetStatus(const char* status) {
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
        happy_until_ms_ = 0;
        gaze_state_ = GazeState::kIdle;
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
        status_neutral_pending_ = true;
        if (emotion_motion_callback_ != nullptr) {
            emotion_motion_callback_("neutral", false);
        }
    }
}

void RobotEyeDisplay::SetEmotionMotionCallback(EmotionMotionCallback callback) {
    DisplayLockGuard lock(this);
    if (lock) {
        emotion_motion_callback_ = callback;
    }
}

void RobotEyeDisplay::NotifyEmotionMotion() {
    if (emotion_motion_callback_ == nullptr) {
        return;
    }
    const Expression expression =
        transition_state_ == TransitionState::kIdle ? current_expression_ : transition_to_;
    const char* emotion = "neutral";
    switch (expression) {
        case Expression::kHappy:
        case Expression::kExcited:
            emotion = "happy";
            break;
        case Expression::kSad:
        case Expression::kDespair:
            emotion = "sad";
            break;
        case Expression::kAngry:
        case Expression::kFurious:
            emotion = "angry";
            break;
        case Expression::kSurprised:
        case Expression::kScared:
            emotion = "surprised";
            break;
        case Expression::kFocused:
        case Expression::kDisoriented:
        case Expression::kWorried:
            emotion = "thinking";
            break;
        case Expression::kWinkLeft:
        case Expression::kWinkRight:
            emotion = "winking";
            break;
        case Expression::kSleepy:
        case Expression::kBored:
        case Expression::kBlinkDown:
            emotion = "sleepy";
            break;
        default:
            break;
    }
    emotion_motion_callback_(emotion, speaking_ && happy_until_ms_ == 0);
}

void RobotEyeDisplay::SetEmotion(const char* emotion) {
    Expression next_expression;
    if (!ParseEmotion(emotion, next_expression)) {
        return;
    }
    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }
    const bool status_reset = status_neutral_pending_ && next_expression == Expression::kNormal;
    status_neutral_pending_ = false;
    if (happy_until_ms_ != 0 && !speaking_ && next_expression == Expression::kNormal) {
        return;
    }
    happy_until_ms_ = 0;
    server_emotion_received_ = !status_reset;
    SetRobotEyeExpression(next_expression);
    NotifyEmotionMotion();
}

void RobotEyeDisplay::ShowHappyFor(uint32_t duration_ms) {
    DisplayLockGuard lock(this);
    if (!lock || speaking_) {
        return;
    }
    requested_screen_ = Screen::kEyes;
    ShowEyesScreen();
    SetRobotEyeExpression(Expression::kHappy);
    happy_until_ms_ = NowMs() + std::max<uint32_t>(1, duration_ms);
    NotifyEmotionMotion();
}

RobotEyeDisplay::Expression RobotEyeDisplay::InferSentenceExpression(const char* content) {
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
            return Expression::kFocused;
        }
    }
    return Expression::kNormal;
}

void RobotEyeDisplay::SetChatMessage(const char* role, const char* content) {
    if (role == nullptr || content == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }
    if (strcmp(role, "user") == 0 && content[0] != '\0') {
        requested_screen_ = DetectRequestedScreen(content);
        if (requested_screen_ == Screen::kTemperature || requested_screen_ == Screen::kWeather) {
            displayed_temperature_valid_ = false;
            weather_kind_ = WeatherKind::kUnknown;
            float temperature = 0.0f;
            if (ParseTemperature(content, temperature)) {
                displayed_temperature_ = temperature;
                displayed_temperature_valid_ = true;
            }
        }
        if (requested_screen_ == Screen::kEyes) {
            ShowEyesScreen();
        } else {
            ShowScreen(requested_screen_, kInfoScreenDurationMs);
        }
        happy_until_ms_ = 0;
        server_emotion_received_ = false;
        response_sentence_received_ = false;
        status_neutral_pending_ = false;
        SetRobotEyeExpression(Expression::kFocused);
        if (emotion_motion_callback_ != nullptr) {
            emotion_motion_callback_("neutral", false);
        }
    } else if (strcmp(role, "assistant") == 0) {
        UpdateInfoFromAssistant(content);
        response_sentence_received_ = content[0] != '\0';
        happy_until_ms_ = 0;
        if (content[0] == '\0') {
            server_emotion_received_ = false;
            SetRobotEyeExpression(Expression::kNormal);
        } else if (!server_emotion_received_) {
            SetRobotEyeExpression(InferSentenceExpression(content));
        }
        NotifyEmotionMotion();
    }
}

void RobotEyeDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }
    server_emotion_received_ = false;
    response_sentence_received_ = false;
    requested_screen_ = Screen::kEyes;
    if (emotion_motion_callback_ != nullptr) {
        emotion_motion_callback_("neutral", false);
    }
    if (happy_until_ms_ == 0) {
        SetRobotEyeExpression(Expression::kNormal);
    }
}

void RobotEyeDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) { (void)image; }
