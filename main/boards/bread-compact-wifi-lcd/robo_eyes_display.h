#pragma once

#include <cstdint>

#include "display/lcd_display.h"

class RoboEyesDisplay : public SpiLcdDisplay {
public:
    RoboEyesDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                    int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                    bool swap_xy);
    ~RoboEyesDisplay() override;

    void SetupUI() override;
    void SetStatus(const char* status) override;
    void SetEmotion(const char* emotion) override;
    void ShowHappyFor(uint32_t duration_ms);
    // Board-owned observer; it must not call back into the display while notified.
    using EmotionMotionCallback = void (*)(const char* emotion, bool speaking);
    void SetEmotionMotionCallback(EmotionMotionCallback callback);
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;

private:
    enum class Expression {
        kNeutral,
        kHappy,
        kAngry,
        kSad,
        kSurprised,
        kThinking,
        kWinking,
        kSleepy,
    };

    struct EyeShape {
        int width;
        int left_height;
        int right_height;
        int brow_height;
    };

    // Match the gaze fields' type; LVGL coordinates can use long on ESP-IDF.
    struct GazeLimits {
        int x;
        int y;
    };

    static void EyeTimerCallback(lv_timer_t* timer);
    static bool ParseEmotion(const char* emotion, Expression& expression);
    static Expression InferSentenceExpression(const char* content);
    EyeShape GetEyeShape() const;
    GazeLimits GetGazeLimits() const;
    void ApplyExpression(Expression expression);
    void NotifyEmotionMotion();
    bool CanMoveGaze() const;
    void CenterGaze();
    void ClampGaze();
    void UpdateEyes();
    void ApplyExpressionColor();

    lv_obj_t* eyes_layer_ = nullptr;
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* left_brow_ = nullptr;
    lv_obj_t* right_brow_ = nullptr;
    lv_point_precise_t left_brow_points_[2] = {};
    lv_point_precise_t right_brow_points_[2] = {};
    lv_timer_t* eye_timer_ = nullptr;
    Expression expression_ = Expression::kNeutral;
    int blink_frame_ = 0;
    int blink_countdown_ = 80;
    int gaze_x_ = 0;
    int gaze_y_ = 0;
    int gaze_target_x_ = 0;
    int gaze_target_y_ = 0;
    int gaze_hold_ticks_ = 6;
    uint32_t happy_ticks_remaining_ = 0;
    bool speaking_ = false;
    bool server_emotion_received_ = false;
    bool response_sentence_received_ = false;
    bool status_neutral_pending_ = false;
    EmotionMotionCallback emotion_motion_callback_ = nullptr;
};
