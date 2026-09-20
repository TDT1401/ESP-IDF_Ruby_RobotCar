#pragma once

#include "display/lcd_display.h"

class RoboEyesDisplay : public SpiLcdDisplay {
public:
    RoboEyesDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                    int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                    bool swap_xy);
    ~RoboEyesDisplay() override;

    void SetupUI() override;
    void SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;

private:
    enum class Expression {
        kNeutral,
        kHappy,
        kAngry,
        kSleepy,
    };

    static void EyeTimerCallback(lv_timer_t* timer);
    void UpdateEyes();
    void ApplyExpressionColor();

    lv_obj_t* eyes_layer_ = nullptr;
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_timer_t* eye_timer_ = nullptr;
    Expression expression_ = Expression::kNeutral;
    int blink_frame_ = 0;
    int blink_countdown_ = 48;
    int gaze_x_ = 0;
    int gaze_y_ = 0;
    int gaze_target_x_ = 0;
    int gaze_target_y_ = 0;
    int gaze_hold_ticks_ = 8;
};
