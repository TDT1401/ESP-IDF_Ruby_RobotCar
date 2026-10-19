#include "robot_eye_display.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <sdkconfig.h>

#include "assets/lang_config.h"
#include "board.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "material_symbols.h"
#include "wifi_manager.h"
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
#include "robot/controller/l298n_controller.h"
#endif

#define TAG "RobotEyeDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(font_material_symbols_30_4);

namespace {
// Timing and geometry are derived from fonzu/RobotEye 1.0.0.
constexpr uint32_t kAnimationPeriodMs = 33;
constexpr uint32_t kInfoScreenDurationMs = 15000;
constexpr uint32_t kInfoRefreshPeriodMs = 500;
constexpr uint32_t kWeatherFramePeriodMs = 100;
constexpr uint32_t kMorphDurationMs = 500;
constexpr uint32_t kBlinkCloseMs = 200;
constexpr uint32_t kBlinkHoldMinMs = 30;
constexpr uint32_t kBlinkHoldMaxMs = 60;
constexpr uint32_t kBlinkOpenMs = 100;
constexpr uint32_t kBlinkIntervalMinMs = 7000;
constexpr uint32_t kBlinkIntervalMaxMs = 12000;
constexpr uint32_t kHalfBlinkPhaseMs = 150;
constexpr uint32_t kHalfBlinkIntervalMinMs = 4000;
constexpr uint32_t kHalfBlinkIntervalMaxMs = 8000;
constexpr uint32_t kGazeTransitionMs = 600;
constexpr uint32_t kGazeHoldMinMs = 1000;
constexpr uint32_t kGazeHoldMaxMs = 2000;
constexpr uint32_t kGazeIntervalMinMs = 4000;
constexpr uint32_t kGazeIntervalMaxMs = 7000;
constexpr uint32_t kDriftIntervalMs = 5000;
constexpr uint32_t kBootClosedMs = 500;
constexpr uint32_t kBootToHalfMs = 1500;
constexpr uint32_t kBootHalfBlinkGapMs = 300;
constexpr uint32_t kBootToFullMs = 1500;
constexpr int kBootBlinkCount = 3;
constexpr int kReferenceEyeWidth = 32;
constexpr int kReferenceEyeHeight = 32;
constexpr int kReferenceEyeRadius = 10;
constexpr int kReferenceEyeGap = 8;
constexpr int kMaxFullRefreshPixels = 160 * 80;

bool DeadlineReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

bool ContainsPhrase(std::string_view text, std::string_view phrase) {
    return std::search(text.begin(), text.end(), phrase.begin(), phrase.end(), [](char a, char b) {
               const auto lower = [](unsigned char character) {
                   return character >= 'A' && character <= 'Z' ? character + 32 : character;
               };
               return lower(a) == lower(b);
           }) != text.end();
}

bool StartsWithPhrase(std::string_view text, std::string_view phrase) {
    if (text.size() < phrase.size()) {
        return false;
    }
    return std::equal(phrase.begin(), phrase.end(), text.begin(), [](char a, char b) {
        const auto lower = [](unsigned char character) {
            return character >= 'A' && character <= 'Z' ? character + 32 : character;
        };
        return lower(a) == lower(b);
    });
}

const char* FindPhraseEnd(const char* text, std::string_view phrase) {
    if (text == nullptr) {
        return nullptr;
    }
    const std::string_view text_view(text);
    const auto match =
        std::search(text_view.begin(), text_view.end(), phrase.begin(), phrase.end(),
                    [](char a, char b) {
                        const auto lower = [](unsigned char character) {
                            return character >= 'A' && character <= 'Z' ? character + 32
                                                                        : character;
                        };
                        return lower(a) == lower(b);
                    });
    if (match == text_view.end()) {
        return nullptr;
    }
    return text + (match - text_view.begin()) + phrase.size();
}
}  // namespace

RobotEyeDisplay::RobotEyeDisplay(esp_lcd_panel_io_handle_t panel_io,
                                 esp_lcd_panel_handle_t panel, int width, int height, int offset_x,
                                 int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    width * height <= kMaxFullRefreshPixels) {
    // Keep enough margin for RobotEye's LOOK_LEFT/LOOK_RIGHT geometry.
    geometry_scale_ = width >= 200 && height >= 200 ? 2 : 1;
    const int total_width = Scale(kReferenceEyeWidth * 2 + kReferenceEyeGap);
    base_left_x_ = static_cast<int16_t>((width_ - total_width) / 2);
    base_right_x_ = static_cast<int16_t>(base_left_x_ +
                                         Scale(kReferenceEyeWidth + kReferenceEyeGap));
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
    lv_canvas_set_palette(eyes_canvas_, 0,
                          lv_color_to_32(lv_color_hex(0x050A12), LV_OPA_COVER));
    lv_canvas_set_palette(eyes_canvas_, 1,
                          lv_color_to_32(lv_color_hex(0x00E5F5), LV_OPA_COVER));
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
        weather_pixels_ =
            static_cast<uint8_t*>(lv_draw_buf_goto_xy(weather_draw_buffer_, 0, 0));
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
        {"neutral", Expression::kNormal},       {"idle", Expression::kNormal},
        {"happy", Expression::kHappy},         {"laughing", Expression::kExcited},
        {"laugh", Expression::kExcited},       {"funny", Expression::kExcited},
        {"loving", Expression::kHappy},        {"embarrassed", Expression::kWorried},
        {"confident", Expression::kFocused},   {"delicious", Expression::kHappy},
        {"silly", Expression::kWinkLeft},      {"winking", Expression::kWinkLeft},
        {"cool", Expression::kFocused},        {"kissy", Expression::kHappy},
        {"sad", Expression::kSad},             {"crying", Expression::kDespair},
        {"angry", Expression::kAngry},         {"mad", Expression::kFurious},
        {"surprised", Expression::kSurprised}, {"shocked", Expression::kScared},
        {"thinking", Expression::kFocused},    {"confused", Expression::kDisoriented},
        {"sleepy", Expression::kSleepy},       {"sleep", Expression::kBlinkDown},
        {"tired", Expression::kBored},         {"relaxed", Expression::kSleepy},
        {"worried", Expression::kWorried},     {"scared", Expression::kScared},
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
    const Expression expression = transition_state_ == TransitionState::kIdle
                                      ? current_expression_
                                      : transition_to_;
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

void RobotEyeDisplay::ShowAlertFor(uint32_t duration_ms) {
    DisplayLockGuard lock(this);
    if (!lock || speaking_)
        return;
    requested_screen_ = Screen::kEyes;
    ShowEyesScreen();
    SetRobotEyeExpression(Expression::kAlert);
    happy_until_ms_ = NowMs() + std::max<uint32_t>(1, duration_ms);
    NotifyEmotionMotion();
}

void RobotEyeDisplay::ShowFindFor(uint32_t duration_ms) {
    DisplayLockGuard lock(this);
    if (!lock)
        return;
    requested_screen_ = Screen::kEyes;
    last_find_flash_ms_ = NowMs();
    find_flash_bright_ = true;
    ShowScreen(Screen::kFind, duration_ms);
    if (info_layer_ != nullptr)
        lv_obj_set_style_bg_color(info_layer_, lv_color_hex(0x17425C), 0);
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

RobotEyeDisplay::Screen RobotEyeDisplay::DetectRequestedScreen(const char* content) {
    if (content == nullptr || content[0] == '\0') {
        return Screen::kEyes;
    }
    const std::string_view text(content);
    for (auto phrase : {"pin", "battery", "phần trăm pin", "mức pin"}) {
        if (ContainsPhrase(text, phrase)) {
            return Screen::kBattery;
        }
    }
    for (auto phrase : {"mấy giờ", "giờ hiện tại", "xem giờ", "what time", "current time"}) {
        if (ContainsPhrase(text, phrase)) {
            return Screen::kClock;
        }
    }
    for (auto phrase : {"wi-fi", "wifi", "trạng thái mạng", "kết nối mạng", "địa chỉ ip",
                        "internet"}) {
        if (ContainsPhrase(text, phrase)) {
            return Screen::kWifi;
        }
    }
    for (auto phrase : {"thời tiết", "dự báo", "hôm nay trời", "trời hôm nay", "trời có",
                        "weather", "forecast"}) {
        if (ContainsPhrase(text, phrase)) {
            return Screen::kWeather;
        }
    }
    for (auto phrase : {"nhiệt độ", "bao nhiêu độ", "độ c", "temperature"}) {
        if (ContainsPhrase(text, phrase)) {
            return Screen::kTemperature;
        }
    }
    return Screen::kEyes;
}

RobotEyeDisplay::WeatherKind RobotEyeDisplay::DetectWeatherKind(const char* content) {
    if (content == nullptr || content[0] == '\0') {
        return WeatherKind::kUnknown;
    }
    const std::string_view text(content);
    for (auto phrase : {"giông", "dông", "sấm", "bão", "thunder", "storm"}) {
        if (ContainsPhrase(text, phrase)) {
            return WeatherKind::kStorm;
        }
    }
    for (auto phrase : {"tuyết", "snow"}) {
        if (ContainsPhrase(text, phrase)) {
            return WeatherKind::kSnow;
        }
    }
    const bool explicitly_dry = ContainsPhrase(text, "không mưa") ||
                                ContainsPhrase(text, "không có mưa") ||
                                ContainsPhrase(text, "no rain") ||
                                ContainsPhrase(text, "no rainfall") ||
                                ContainsPhrase(text, "without rain");
    if (!explicitly_dry) {
        for (auto phrase : {"mưa", "mưa rào", "rain", "showers", "drizzle"}) {
            if (ContainsPhrase(text, phrase)) {
                return WeatherKind::kRain;
            }
        }
    }
    for (auto phrase : {"sương mù", "sương", "fog", "mist", "haze"}) {
        if (ContainsPhrase(text, phrase)) {
            return WeatherKind::kFog;
        }
    }
    const bool explicitly_cloudless = ContainsPhrase(text, "không có mây") ||
                                      ContainsPhrase(text, "không mây") ||
                                      ContainsPhrase(text, "cloudless");
    if (!explicitly_cloudless) {
        for (auto phrase : {"nhiều mây", "có mây", "mây", "u ám", "cloud", "overcast"}) {
            if (ContainsPhrase(text, phrase)) {
                return WeatherKind::kCloudy;
            }
        }
    }
    for (auto phrase : {"nắng", "quang", "trời trong", "sunny", "clear"}) {
        if (ContainsPhrase(text, phrase)) {
            return WeatherKind::kClear;
        }
    }
    return WeatherKind::kUnknown;
}

bool RobotEyeDisplay::ParsePercentage(const char* content, int& percentage) {
    if (content == nullptr) {
        return false;
    }
    for (const char* cursor = content; *cursor != '\0'; ++cursor) {
        if (!std::isdigit(static_cast<unsigned char>(*cursor))) {
            continue;
        }
        char* end = nullptr;
        const long value = std::strtol(cursor, &end, 10);
        const char* suffix = end;
        while (*suffix != '\0' && std::isspace(static_cast<unsigned char>(*suffix))) {
            ++suffix;
        }
        const std::string_view suffix_text(suffix);
        const bool has_percentage_unit =
            *suffix == '%' || StartsWithPhrase(suffix_text, "phần trăm") ||
            StartsWithPhrase(suffix_text, "percent");
        if (has_percentage_unit && value >= 0 && value <= 100) {
            percentage = static_cast<int>(value);
            return true;
        }
        cursor = end - 1;
    }
    return false;
}

bool RobotEyeDisplay::ParseTemperature(const char* content, float& temperature) {
    if (content == nullptr) {
        return false;
    }
    for (const char* cursor = content; *cursor != '\0'; ++cursor) {
        if (!std::isdigit(static_cast<unsigned char>(*cursor)) &&
            !(*cursor == '-' && std::isdigit(static_cast<unsigned char>(cursor[1])))) {
            continue;
        }
        char* end = nullptr;
        const float value = std::strtof(cursor, &end);
        const char* suffix = end;
        while (*suffix != '\0' && std::isspace(static_cast<unsigned char>(*suffix))) {
            ++suffix;
        }
        const bool degree_symbol = static_cast<unsigned char>(suffix[0]) == 0xC2 &&
                                   static_cast<unsigned char>(suffix[1]) == 0xB0;
        const std::string_view suffix_text(suffix);
        const bool has_temperature_unit =
            degree_symbol || StartsWithPhrase(suffix_text, "℃") ||
            StartsWithPhrase(suffix_text, "º") || StartsWithPhrase(suffix_text, "độ") ||
            StartsWithPhrase(suffix_text, "degrees") ||
            StartsWithPhrase(suffix_text, "celsius") ||
            ((suffix[0] == 'c' || suffix[0] == 'C') &&
             (suffix[1] == '\0' || std::isspace(static_cast<unsigned char>(suffix[1])) ||
              std::ispunct(static_cast<unsigned char>(suffix[1]))));
        if (has_temperature_unit && value >= -100.0f && value <= 150.0f) {
            temperature = value;
            return true;
        }
        cursor = end - 1;
    }

    // Some weather providers omit the unit in text such as "Nhiệt độ hiện tại: 28".
    // In that case, only accept the first plausible number after the temperature keyword.
    const char* value_start = FindPhraseEnd(content, "nhiệt độ");
    if (value_start == nullptr) {
        value_start = FindPhraseEnd(content, "temperature");
    }
    if (value_start != nullptr) {
        while (*value_start != '\0' && !std::isdigit(static_cast<unsigned char>(*value_start)) &&
               !(*value_start == '-' &&
                 std::isdigit(static_cast<unsigned char>(value_start[1])))) {
            ++value_start;
        }
        if (*value_start != '\0') {
            char* end = nullptr;
            const float value = std::strtof(value_start, &end);
            if (end != value_start && value >= -100.0f && value <= 150.0f) {
                temperature = value;
                return true;
            }
        }
    }
    return false;
}

void RobotEyeDisplay::ShowScreen(Screen screen, uint32_t duration_ms) {
    if (screen == Screen::kEyes) {
        ShowEyesScreen();
        return;
    }
    if (active_screen_ == Screen::kFind && screen != Screen::kFind && info_layer_ != nullptr)
        lv_obj_set_style_bg_color(info_layer_, lv_color_hex(0x050A12), 0);
    active_screen_ = screen;
    screen_until_ms_ = NowMs() + duration_ms;
    if (info_layer_ != nullptr) {
        lv_obj_remove_flag(info_layer_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(info_layer_);
    }
    const bool uses_weather_canvas =
        (screen == Screen::kTemperature || screen == Screen::kWeather) &&
        weather_canvas_ != nullptr;
    if (weather_canvas_ != nullptr) {
        if (uses_weather_canvas) {
            lv_obj_remove_flag(weather_canvas_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(weather_canvas_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (info_icon_label_ != nullptr) {
        if (uses_weather_canvas) {
            lv_obj_add_flag(info_icon_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(info_icon_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    RenderWeatherScene(NowMs(), true);
    RefreshInfoScreen(NowMs(), true);
}

void RobotEyeDisplay::ShowEyesScreen() {
    if (active_screen_ == Screen::kFind && info_layer_ != nullptr)
        lv_obj_set_style_bg_color(info_layer_, lv_color_hex(0x050A12), 0);
    active_screen_ = Screen::kEyes;
    screen_until_ms_ = 0;
    if (info_layer_ != nullptr) {
        lv_obj_add_flag(info_layer_, LV_OBJ_FLAG_HIDDEN);
    }
}

void RobotEyeDisplay::SetInfoText(const char* icon, const char* title, const char* value,
                                  const char* detail) {
    if (info_icon_label_ == nullptr) {
        return;
    }
    lv_label_set_text(info_icon_label_, icon);
    lv_label_set_text(info_title_label_, title);
    lv_label_set_text(info_value_label_, value);
    lv_label_set_text(info_detail_label_, detail);
}

void RobotEyeDisplay::ClearWeatherCanvas() {
    if (weather_pixels_ != nullptr) {
        memset(weather_pixels_, 0, weather_stride_ * weather_canvas_height_);
    }
}

void RobotEyeDisplay::SetWeatherPixel(int x, int y, bool enabled) {
    if (weather_pixels_ == nullptr || x < 0 || x >= weather_canvas_width_ || y < 0 ||
        y >= weather_canvas_height_) {
        return;
    }
    uint8_t& value = weather_pixels_[y * weather_stride_ + (x >> 3)];
    const uint8_t mask = static_cast<uint8_t>(0x80U >> (x & 7));
    if (enabled) {
        value |= mask;
    } else {
        value &= static_cast<uint8_t>(~mask);
    }
}

void RobotEyeDisplay::DrawWeatherBox(int x, int y, int width, int height) {
    for (int row = std::max(0, y); row < std::min(weather_canvas_height_, y + height); ++row) {
        for (int column = std::max(0, x);
             column < std::min(weather_canvas_width_, x + width); ++column) {
            SetWeatherPixel(column, row);
        }
    }
}

void RobotEyeDisplay::DrawWeatherCircle(int center_x, int center_y, int radius) {
    const int radius_squared = radius * radius;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= radius_squared) {
                SetWeatherPixel(center_x + x, center_y + y);
            }
        }
    }
}

void RobotEyeDisplay::DrawWeatherLine(int x0, int y0, int x1, int y1, int thickness) {
    const int delta_x = std::abs(x1 - x0);
    const int step_x = x0 < x1 ? 1 : -1;
    const int delta_y = -std::abs(y1 - y0);
    const int step_y = y0 < y1 ? 1 : -1;
    int error = delta_x + delta_y;
    while (true) {
        const int radius = std::max(0, thickness - 1);
        DrawWeatherBox(x0 - radius, y0 - radius, radius * 2 + 1, radius * 2 + 1);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int doubled_error = error * 2;
        if (doubled_error >= delta_y) {
            error += delta_y;
            x0 += step_x;
        }
        if (doubled_error <= delta_x) {
            error += delta_x;
            y0 += step_y;
        }
    }
}

void RobotEyeDisplay::DrawWeatherCloud(int y_offset) {
    const int unit = std::min(weather_canvas_width_, weather_canvas_height_);
    const int base_x = weather_canvas_width_ * 18 / 100;
    const int base_y = weather_canvas_height_ * 45 / 100 + y_offset;
    const int base_width = weather_canvas_width_ * 64 / 100;
    const int base_height = std::max(5, weather_canvas_height_ * 18 / 100);
    DrawWeatherCircle(weather_canvas_width_ * 38 / 100, base_y, std::max(5, unit * 13 / 100));
    DrawWeatherCircle(weather_canvas_width_ * 57 / 100, base_y - unit * 7 / 100,
                      std::max(7, unit * 18 / 100));
    DrawWeatherCircle(weather_canvas_width_ * 72 / 100, base_y + unit * 2 / 100,
                      std::max(5, unit * 12 / 100));
    DrawWeatherBox(base_x, base_y, base_width, base_height);
}

void RobotEyeDisplay::RenderWeatherScene(uint32_t now, bool force) {
    if (weather_canvas_ == nullptr || weather_draw_buffer_ == nullptr ||
        weather_pixels_ == nullptr ||
        (active_screen_ != Screen::kTemperature && active_screen_ != Screen::kWeather)) {
        return;
    }
    if (!force && now - last_weather_frame_ms_ < kWeatherFramePeriodMs) {
        return;
    }
    last_weather_frame_ms_ = now;
    ClearWeatherCanvas();

    const int unit = std::min(weather_canvas_width_, weather_canvas_height_);
    const int center_x = weather_canvas_width_ / 2;
    const int center_y = weather_canvas_height_ / 2;
    if (active_screen_ == Screen::kTemperature || weather_kind_ == WeatherKind::kUnknown) {
        const int bulb_radius = std::max(5, unit / 9);
        const int stem_width = std::max(4, unit / 10);
        const int stem_height = std::max(18, unit * 42 / 100);
        DrawWeatherCircle(center_x, center_y + stem_height / 3, bulb_radius);
        DrawWeatherBox(center_x - stem_width / 2, center_y - stem_height / 2, stem_width,
                       stem_height);
        DrawWeatherCircle(center_x, center_y - stem_height / 2, stem_width / 2);
    } else if (weather_kind_ == WeatherKind::kClear) {
        const int radius = std::max(7, unit / 5);
        DrawWeatherCircle(center_x, center_y, radius);
        for (int angle = 0; angle < 8; ++angle) {
            const float radians = static_cast<float>(angle) * 3.14159265f / 4.0f;
            const int inner = radius + 5;
            const int outer = radius + std::max(9, unit / 6);
            DrawWeatherLine(center_x + static_cast<int>(std::cos(radians) * inner),
                            center_y + static_cast<int>(std::sin(radians) * inner),
                            center_x + static_cast<int>(std::cos(radians) * outer),
                            center_y + static_cast<int>(std::sin(radians) * outer), 1);
        }
    } else if (weather_kind_ == WeatherKind::kCloudy) {
        DrawWeatherCircle(weather_canvas_width_ * 72 / 100, weather_canvas_height_ * 28 / 100,
                          std::max(5, unit / 9));
        DrawWeatherCloud(unit / 14);
    } else if (weather_kind_ == WeatherKind::kFog) {
        DrawWeatherCloud(-unit / 8);
        const int drift = static_cast<int>((now / 300) % 7);
        for (int line = 0; line < 3; ++line) {
            const int y = weather_canvas_height_ * 68 / 100 + line * std::max(4, unit / 11);
            const int x = weather_canvas_width_ * 16 / 100 + ((line + drift) % 3) * 3;
            DrawWeatherLine(x, y, weather_canvas_width_ - x, y, 1);
        }
    } else {
        DrawWeatherCloud(-unit / 7);
        if (weather_kind_ == WeatherKind::kStorm) {
            const int x = center_x;
            const int top = weather_canvas_height_ * 58 / 100;
            DrawWeatherLine(x + unit / 10, top, x - unit / 15, top + unit / 5, 2);
            DrawWeatherLine(x - unit / 15, top + unit / 5, x + unit / 18,
                            top + unit * 38 / 100, 2);
        } else {
            const int phase = static_cast<int>((now / kWeatherFramePeriodMs) % 12);
            for (int drop = 0; drop < 5; ++drop) {
                const int x = weather_canvas_width_ * (20 + drop * 15) / 100;
                const int y = weather_canvas_height_ * 58 / 100 +
                              (phase + drop * 5) % std::max(8, weather_canvas_height_ * 30 / 100);
                if (weather_kind_ == WeatherKind::kSnow) {
                    DrawWeatherLine(x - 2, y, x + 2, y, 1);
                    DrawWeatherLine(x, y - 2, x, y + 2, 1);
                } else {
                    DrawWeatherLine(x + 2, y, x - 2, y + std::max(4, unit / 10), 1);
                }
            }
        }
    }

    lv_draw_buf_flush_cache(weather_draw_buffer_, nullptr);
    lv_obj_invalidate(weather_canvas_);
}

void RobotEyeDisplay::RefreshInfoScreen(uint32_t now, bool force) {
    if (active_screen_ == Screen::kEyes || (!force && now - last_info_refresh_ms_ <
                                                        kInfoRefreshPeriodMs)) {
        return;
    }
    last_info_refresh_ms_ = now;
    char value[64] = {};
    char detail[256] = {};
    switch (active_screen_) {
        case Screen::kBattery: {
            int level = 0;
            bool charging = false;
            bool discharging = false;
            const bool has_sensor =
                Board::GetInstance().GetBatteryLevel(level, charging, discharging);
            if (has_sensor) {
                displayed_battery_level_ = std::clamp(level, 0, 100);
            }
            const char* icon = MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_ALERT;
            if (charging) {
                icon = MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_BOLT;
            } else if (displayed_battery_level_ >= 90) {
                icon = MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_FULL;
            } else if (displayed_battery_level_ >= 65) {
                icon = MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_5;
            } else if (displayed_battery_level_ >= 40) {
                icon = MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_3;
            } else if (displayed_battery_level_ >= 15) {
                icon = MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_1;
            } else if (displayed_battery_level_ >= 0) {
                icon = MATERIAL_SYMBOLS_BATTERY_ANDROID_0;
            }
            if (displayed_battery_level_ >= 0) {
                snprintf(value, sizeof(value), "Battery: %d%%", displayed_battery_level_);
                if (has_sensor) {
                    snprintf(detail, sizeof(detail), "%s",
                             charging ? "Charging" : (discharging ? "On battery" : "Battery"));
                } else {
                    snprintf(detail, sizeof(detail), "AI response");
                }
            } else {
                snprintf(value, sizeof(value), "Battery: --%%");
                snprintf(detail, sizeof(detail), "No battery sensor");
            }
            SetInfoText(icon, "BATTERY", value, detail);
            break;
        }
        case Screen::kClock: {
            const time_t current_time = time(nullptr);
            struct tm local_time = {};
            localtime_r(&current_time, &local_time);
            if (local_time.tm_year >= 2025 - 1900) {
                strftime(value, sizeof(value), "%H:%M", &local_time);
                strftime(detail, sizeof(detail), "%d/%m/%Y", &local_time);
            } else {
                snprintf(value, sizeof(value), "--:--");
                snprintf(detail, sizeof(detail), "Time not synced");
            }
            SetInfoText(MATERIAL_SYMBOLS_SCHEDULE, "CLOCK", value, detail);
            break;
        }
        case Screen::kWifi: {
            auto& wifi = WifiManager::GetInstance();
            if (wifi.IsConnected()) {
                snprintf(value, sizeof(value), "Wi-Fi: Connected");
                const std::string ssid = wifi.GetSsid();
                const std::string ip = wifi.GetIpAddress();
                snprintf(detail, sizeof(detail), "%s\n%s", ssid.c_str(), ip.c_str());
                SetInfoText(MATERIAL_SYMBOLS_WIFI, "WI-FI", value, detail);
            } else {
                snprintf(value, sizeof(value), "Wi-Fi: Disconnected");
                SetInfoText(MATERIAL_SYMBOLS_WIFI_OFF, "WI-FI", value, "No network");
            }
            break;
        }
        case Screen::kTemperature:
        case Screen::kWeather: {
            float temperature = 0.0f;
            const bool has_sensor = Board::GetInstance().GetTemperature(temperature);
            if (has_sensor) {
                displayed_temperature_ = temperature;
                displayed_temperature_valid_ = true;
            }
            if (displayed_temperature_valid_) {
                if (std::fabs(displayed_temperature_ - std::round(displayed_temperature_)) < 0.05f) {
                    snprintf(value, sizeof(value), "%.0f\xC2\xB0" "C", displayed_temperature_);
                } else {
                    snprintf(value, sizeof(value), "%.1f\xC2\xB0" "C", displayed_temperature_);
                }
            } else {
                snprintf(value, sizeof(value), "--\xC2\xB0" "C");
            }
            const char* weather_label = "LIVE WEATHER";
            switch (weather_kind_) {
                case WeatherKind::kClear:
                    weather_label = "SUNNY";
                    break;
                case WeatherKind::kCloudy:
                    weather_label = "CLOUDY";
                    break;
                case WeatherKind::kRain:
                    weather_label = "RAIN";
                    break;
                case WeatherKind::kStorm:
                    weather_label = "THUNDERSTORM";
                    break;
                case WeatherKind::kFog:
                    weather_label = "FOG";
                    break;
                case WeatherKind::kSnow:
                    weather_label = "SNOW";
                    break;
                case WeatherKind::kUnknown:
                    weather_label = displayed_temperature_valid_ ? "CURRENT WEATHER"
                                                                 : "WAITING FOR WEATHER";
                    break;
            }
            if (active_screen_ == Screen::kTemperature) {
                weather_label = "CURRENT TEMPERATURE";
            }
            SetInfoText(MATERIAL_SYMBOLS_DEVICE_THERMOSTAT,
                        active_screen_ == Screen::kWeather ? "WEATHER" : "TEMP", value,
                        weather_label);
            break;
        }
        case Screen::kDrive: {
#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
            const auto motor = GetL298nMotorStatus();
            const char* direction = "STOP";
            if (strcmp(motor.motion, "forward") == 0)
                direction = "FORWARD";
            else if (strcmp(motor.motion, "backward") == 0)
                direction = "BACK";
            else if (strcmp(motor.motion, "turn_left") == 0)
                direction = "LEFT";
            else if (strcmp(motor.motion, "turn_right") == 0)
                direction = "RIGHT";
            snprintf(value, sizeof(value), "%s %d%%", direction, motor.active_speed_percent);
            const char* owner = strcmp(motor.control_owner, "web") == 0 ? "WEB" :
                                strcmp(motor.control_owner, "telegram") == 0 ? "TELEGRAM" :
                                strcmp(motor.control_owner, "voice") == 0 ? "VOICE" : "AUTO";
            char range[20];
            if (motor.distance_valid)
                snprintf(range, sizeof(range), "%dcm", motor.measured_distance_cm);
            else
                snprintf(range, sizeof(range), "--");
            const bool warning = motor.distance_valid &&
                                 motor.measured_distance_cm <
                                     (motor.web_manual ? motor.web_warning_effective_cm :
                                                         motor.obstacle_stop_effective_cm);
            snprintf(detail, sizeof(detail), "SR04 %s  |  %s\nBEEP %s%s", range, owner,
                     motor.web_manual ? (motor.warning_sound_enabled ? "ON" : "OFF") : "--",
                     warning ? "  ! NEAR" : "");
            SetInfoText(MATERIAL_SYMBOLS_WIFI, "RUBY DRIVE", value, detail);
#endif
            break;
        }
        case Screen::kFind: {
            auto& wifi = WifiManager::GetInstance();
            const std::string ip = wifi.IsConnected() ? wifi.GetIpAddress() : "No IP";
            SetInfoText(MATERIAL_SYMBOLS_WIFI, "FIND RUBY", ip.c_str(), "Ruby is here!");
            break;
        }
        case Screen::kEyes:
            break;
    }
}

void RobotEyeDisplay::UpdateInfoFromAssistant(const char* content) {
    if (requested_screen_ == Screen::kBattery) {
        int percentage = 0;
        if (ParsePercentage(content, percentage)) {
            displayed_battery_level_ = percentage;
        }
    } else if (requested_screen_ == Screen::kTemperature ||
               requested_screen_ == Screen::kWeather) {
        const WeatherKind detected_weather = DetectWeatherKind(content);
        if (detected_weather != WeatherKind::kUnknown) {
            weather_kind_ = detected_weather;
        }
        float temperature = 0.0f;
        if (ParseTemperature(content, temperature)) {
            displayed_temperature_ = temperature;
            displayed_temperature_valid_ = true;
        }
    }
    if (requested_screen_ != Screen::kEyes) {
        ShowScreen(requested_screen_, kInfoScreenDurationMs);
    }
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
        if (requested_screen_ == Screen::kBattery) {
            displayed_battery_level_ = -1;
            int percentage = 0;
            if (ParsePercentage(content, percentage)) {
                displayed_battery_level_ = percentage;
            }
        } else if (requested_screen_ == Screen::kTemperature ||
                   requested_screen_ == Screen::kWeather) {
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

float RobotEyeDisplay::Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

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

uint32_t RobotEyeDisplay::NowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

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
                            Scale(32), Scale(32), Scale(10), 0, 0, 0, 0};
    const EyeParams right = {static_cast<int16_t>(base_right_x_ + drift_x),
                             static_cast<int16_t>(base_y_ + drift_y),
                             Scale(32), Scale(32), Scale(10), 0, 0, 0, 0};
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
        const EyeParams left = {base_left_x_, static_cast<int16_t>(base_y_ + Scale(15)), Scale(32),
                                Scale(2), Scale(1), 0, 0, 0, 0};
        const EyeParams right = {base_right_x_, static_cast<int16_t>(base_y_ + Scale(15)),
                                 Scale(32), Scale(2), Scale(1), 0, 0, 0, 0};
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
            float amount = Clamp01(static_cast<float>(now - boot_phase_started_ms_) /
                                   kBootToHalfMs);
            face = InterpolateFace(closed, half, EaseInOut(amount));
        } else {
            float amount = Clamp01(static_cast<float>(now - boot_phase_started_ms_) /
                                   kBootToFullMs);
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
        blink_amount = EaseInOut(Clamp01(static_cast<float>(now - blink_started_ms_) /
                                         kBlinkCloseMs));
    } else if (blink_state_ == BlinkState::kHold) {
        blink_amount = 1.0f;
    } else if (blink_state_ == BlinkState::kOpening) {
        blink_amount =
            1.0f - EaseInOut(Clamp01(static_cast<float>(now - blink_started_ms_) / kBlinkOpenMs));
    }
    face = ApplyBlink(face, blink_amount, false);

    float half_blink_amount = 0.0f;
    if (half_blink_state_ == HalfBlinkState::kShrinking) {
        half_blink_amount = EaseInOut(Clamp01(
            static_cast<float>(now - half_blink_started_ms_) / kHalfBlinkPhaseMs));
    } else if (half_blink_state_ == HalfBlinkState::kExpanding) {
        half_blink_amount = 1.0f - EaseInOut(Clamp01(
                                             static_cast<float>(now - half_blink_started_ms_) /
                                             kHalfBlinkPhaseMs));
    }
    return ApplyBlink(face, half_blink_amount, true);
}

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
    if (speaking_ || boot_phase_ != BootPhase::kIdle || current_expression_ != Expression::kNormal ||
        transition_state_ != TransitionState::kIdle) {
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
        const auto motor = GetL298nMotorStatus();
        const bool driving = motor.web_manual || motor.pending || motor.moving ||
                             strcmp(motor.autonomy_mode, "manual") != 0 ||
                             strcmp(motor.route_state, "running") == 0 ||
                             strcmp(motor.scan_state, "scanning") == 0;
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
            if (active_screen_ == Screen::kFind && info_layer_ != nullptr &&
                now - last_find_flash_ms_ >= 300) {
                last_find_flash_ms_ = now;
                find_flash_bright_ = !find_flash_bright_;
                lv_obj_set_style_bg_color(info_layer_,
                                          lv_color_hex(find_flash_bright_ ? 0x17425C : 0x050A12), 0);
            }
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
        const float amount = EaseInOut(
            Clamp01(static_cast<float>(now - transition_started_ms_) / kMorphDurationMs));
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

void RobotEyeDisplay::ClearCanvas() {
    if (canvas_pixels_ != nullptr) {
        memset(canvas_pixels_, 0, canvas_stride_ * height_);
    }
}

void RobotEyeDisplay::SetPixel(int x, int y, bool enabled) {
    if (canvas_pixels_ == nullptr || x < 0 || x >= width_ || y < 0 || y >= height_) {
        return;
    }
    uint8_t& value = canvas_pixels_[y * canvas_stride_ + (x >> 3)];
    const uint8_t mask = static_cast<uint8_t>(0x80U >> (x & 7));
    if (enabled) {
        value |= mask;
    } else {
        value &= static_cast<uint8_t>(~mask);
    }
}

void RobotEyeDisplay::DrawBox(int x, int y, int width, int height, bool enabled) {
    const int left = std::max(0, x);
    const int top = std::max(0, y);
    const int right = std::min(width_, x + width);
    const int bottom = std::min(height_, y + height);
    for (int row = top; row < bottom; ++row) {
        for (int column = left; column < right; ++column) {
            SetPixel(column, row, enabled);
        }
    }
}

void RobotEyeDisplay::DrawRoundedBox(int x, int y, int width, int height, int radius,
                                     bool enabled) {
    if (width <= 0 || height <= 0) {
        return;
    }
    radius = std::clamp(radius, 0, std::min(width, height) / 2);
    if (radius == 0) {
        DrawBox(x, y, width, height, enabled);
        return;
    }
    const int radius_squared = radius * radius;
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            int delta_x = 0;
            int delta_y = 0;
            if (column < radius) {
                delta_x = radius - 1 - column;
            } else if (column >= width - radius) {
                delta_x = column - (width - radius);
            }
            if (row < radius) {
                delta_y = radius - 1 - row;
            } else if (row >= height - radius) {
                delta_y = row - (height - radius);
            }
            if (delta_x == 0 || delta_y == 0 ||
                delta_x * delta_x + delta_y * delta_y <= radius_squared) {
                SetPixel(x + column, y + row, enabled);
            }
        }
    }
}

void RobotEyeDisplay::DrawEye(const EyeParams& eye) {
    if (eye.width <= 0 || eye.height <= 0) {
        return;
    }
    DrawRoundedBox(eye.x, eye.y, eye.width, eye.height, eye.radius, true);
    if (eye.top_cut > 0 && eye.top_cut < eye.height) {
        DrawBox(eye.x, eye.y, eye.width, eye.top_cut, false);
    }
    if (eye.bottom_cut > 0 && eye.bottom_cut < eye.height) {
        DrawBox(eye.x, eye.y + eye.height - eye.bottom_cut, eye.width, eye.bottom_cut, false);
    }
    if (eye.top_slope_left <= 0 && eye.top_slope_right <= 0) {
        return;
    }
    const int minimum = std::min({eye.top_slope_left, eye.top_slope_right, eye.height});
    const int maximum = std::min(std::max(eye.top_slope_left, eye.top_slope_right), eye.height);
    if (minimum > 0) {
        DrawBox(eye.x, eye.y, eye.width, minimum, false);
    }
    const int span = maximum - minimum;
    for (int row = 0; row < span; ++row) {
        const int black_width = eye.width - eye.width * row / span;
        const int start_x = eye.top_slope_right > eye.top_slope_left
                                ? eye.x + eye.width - black_width
                                : eye.x;
        DrawBox(start_x, eye.y + minimum + row, black_width, 1, false);
    }
}

void RobotEyeDisplay::DrawFace(const FaceParams& face) {
    DrawEye(face.left);
    DrawEye(face.right);
}

void RobotEyeDisplay::DrawExcitedMask(const FaceParams& face, float amount) {
    const int radius_x = std::max(1, static_cast<int>(Scale(25) * amount));
    const int radius_y = std::max(1, static_cast<int>(Scale(12) * amount));
    const auto draw_mask = [&](const EyeParams& eye) {
        const int center_x = eye.x + eye.width / 2;
        const int center_y = eye.y + eye.height / 2;
        for (int row = -radius_y; row <= radius_y; ++row) {
            const float normalized = static_cast<float>(row * row) /
                                     static_cast<float>(radius_y * radius_y);
            const int half_width =
                static_cast<int>(radius_x * std::sqrt(std::max(0.0f, 1.0f - normalized)));
            DrawBox(center_x - half_width, center_y + row, half_width * 2, 1, false);
        }
    };
    draw_mask(face.left);
    draw_mask(face.right);
}

void RobotEyeDisplay::PresentCanvas() {
    lv_draw_buf_flush_cache(canvas_draw_buffer_, nullptr);
    lv_obj_invalidate(eyes_canvas_);
}
