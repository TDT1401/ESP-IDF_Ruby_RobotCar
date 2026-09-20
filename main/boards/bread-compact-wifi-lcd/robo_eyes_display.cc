#include "robo_eyes_display.h"

#include <algorithm>
#include <cstring>

#include <esp_log.h>
#include <esp_random.h>

#include "display/lvgl_display/lvgl_theme.h"

#define TAG "RoboEyesDisplay"

namespace {
constexpr int kEyeWidth = 28;
constexpr int kEyeHeight = 46;
constexpr int kEyeGap = 18;
constexpr int kEyeTop = 15;
constexpr int kClosedEyeHeight = 4;
constexpr int kEyeTimerPeriodMs = 80;
}  // namespace

RoboEyesDisplay::RoboEyesDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                 int width, int height, int offset_x, int offset_y, bool mirror_x,
                                 bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    true) {}

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
    lv_obj_set_size(eyes_layer_, width_, height_);
    lv_obj_set_style_bg_color(eyes_layer_, lv_color_hex(0x050A12), 0);
    lv_obj_set_style_bg_opa(eyes_layer_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eyes_layer_, 0, 0);
    lv_obj_set_style_pad_all(eyes_layer_, 0, 0);
    lv_obj_set_scrollbar_mode(eyes_layer_, LV_SCROLLBAR_MODE_OFF);

    left_eye_ = lv_obj_create(eyes_layer_);
    right_eye_ = lv_obj_create(eyes_layer_);

    for (auto* eye : {left_eye_, right_eye_}) {
        lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_shadow_width(eye, 0, 0);
        lv_obj_set_style_shadow_opa(eye, LV_OPA_TRANSP, 0);
    }

    ApplyExpressionColor();
    UpdateEyes();
    eye_timer_ = lv_timer_create(EyeTimerCallback, kEyeTimerPeriodMs, this);
    ESP_LOGI(TAG, "Robo eyes ready");
}

void RoboEyesDisplay::SetEmotion(const char* emotion) {
    Expression next_expression = Expression::kNeutral;
    if (emotion != nullptr) {
        if (strstr(emotion, "happy") != nullptr || strstr(emotion, "laugh") != nullptr) {
            next_expression = Expression::kHappy;
        } else if (strstr(emotion, "angry") != nullptr || strstr(emotion, "mad") != nullptr) {
            next_expression = Expression::kAngry;
        } else if (strstr(emotion, "sleep") != nullptr || strstr(emotion, "tired") != nullptr) {
            next_expression = Expression::kSleepy;
        }
    }

    DisplayLockGuard lock(this);
    expression_ = next_expression;
    if (left_eye_ != nullptr) {
        ApplyExpressionColor();
        UpdateEyes();
    }
}

void RoboEyesDisplay::SetChatMessage(const char* role, const char* content) {
    (void)role;
    (void)content;
}

void RoboEyesDisplay::ClearChatMessages() {}

void RoboEyesDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) { (void)image; }

void RoboEyesDisplay::EyeTimerCallback(lv_timer_t* timer) {
    auto* display = static_cast<RoboEyesDisplay*>(lv_timer_get_user_data(timer));
    if (display == nullptr || display->left_eye_ == nullptr) {
        return;
    }

    if (display->expression_ == Expression::kSleepy) {
        display->blink_frame_ = 3;
    } else if (display->blink_frame_ > 0) {
        if (++display->blink_frame_ > 6) {
            display->blink_frame_ = 0;
            display->blink_countdown_ = 48 + static_cast<int>(esp_random() % 48);
        }
    } else if (--display->blink_countdown_ <= 0) {
        display->blink_frame_ = 1;
    }

    if (display->gaze_x_ == display->gaze_target_x_ &&
        display->gaze_y_ == display->gaze_target_y_ && --display->gaze_hold_ticks_ <= 0) {
        static constexpr int kGazeTargets[][2] = {
            {0, 0}, {12, 0}, {-12, 0}, {0, -8}, {0, 8}, {10, -6}, {-10, -6}, {10, 6}, {-10, 6},
        };
        const auto& target =
            kGazeTargets[esp_random() % (sizeof(kGazeTargets) / sizeof(kGazeTargets[0]))];
        display->gaze_target_x_ = target[0];
        display->gaze_target_y_ = target[1];
        display->gaze_hold_ticks_ = 10 + static_cast<int>(esp_random() % 16);
    }

    if (display->gaze_x_ < display->gaze_target_x_) {
        ++display->gaze_x_;
    } else if (display->gaze_x_ > display->gaze_target_x_) {
        --display->gaze_x_;
    }
    if (display->gaze_y_ < display->gaze_target_y_) {
        ++display->gaze_y_;
    } else if (display->gaze_y_ > display->gaze_target_y_) {
        --display->gaze_y_;
    }

    display->UpdateEyes();
}

void RoboEyesDisplay::ApplyExpressionColor() {
    const lv_color_t eye_color = lv_color_hex(0x00E5F5);

    for (auto* eye : {left_eye_, right_eye_}) {
        lv_obj_set_style_bg_color(eye, eye_color, 0);
        lv_obj_set_style_shadow_color(eye, eye_color, 0);
    }
}

void RoboEyesDisplay::UpdateEyes() {
    int eye_height = kEyeHeight;
    if (expression_ == Expression::kSleepy) {
        eye_height = kClosedEyeHeight;
    } else if (blink_frame_ > 0) {
        const int closing = blink_frame_ <= 3 ? blink_frame_ : 6 - blink_frame_;
        eye_height = std::max(kClosedEyeHeight, kEyeHeight - closing * (kEyeHeight / 3));
    }

    const int total_width = 2 * kEyeWidth + kEyeGap;
    const int left_x = (width_ - total_width) / 2;
    const int right_x = left_x + kEyeWidth + kEyeGap;
    const int eye_y = kEyeTop + (kEyeHeight - eye_height) / 2;

    lv_obj_set_size(left_eye_, kEyeWidth, eye_height);
    lv_obj_set_size(right_eye_, kEyeWidth, eye_height);
    lv_obj_set_pos(left_eye_, left_x + gaze_x_, eye_y + gaze_y_);
    lv_obj_set_pos(right_eye_, right_x + gaze_x_, eye_y + gaze_y_);
}
