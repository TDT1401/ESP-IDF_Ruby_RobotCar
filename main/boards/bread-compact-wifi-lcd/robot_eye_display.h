#pragma once

#include <cstdint>
#include "display/lcd_display.h"

// LVGL port of fonzu/RobotEye's vector geometry and non-blocking animation model.
// The upstream library targets Arduino/U8g2; this display keeps the XiaoZhi ESP-IDF
// LCD stack and renders the same style of vector eyes into a 1-bit LVGL canvas.
class RobotEyeDisplay : public SpiLcdDisplay {
public:
    RobotEyeDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                    int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                    bool swap_xy);
    ~RobotEyeDisplay() override;

    void SetupUI() override;
    void SetStatus(const char* status) override;
    void SetEmotion(const char* emotion) override;
    void ShowHappyFor(uint32_t duration_ms);
    void ShowAlertFor(uint32_t duration_ms);
    void ShowFindFor(uint32_t duration_ms);

    // Board-owned observer; it must not call back into the display while notified.
    using EmotionMotionCallback = void (*)(const char* emotion, bool speaking);
    void SetEmotionMotionCallback(EmotionMotionCallback callback);
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;

private:
    // Names follow RobotEye's public expression enum. Only vector expressions are used here;
    // bitmap expressions from Irisoled are intentionally excluded from the TFT port.
    enum class Expression : uint8_t {
        kNormal,
        kAlert,
        kAngry,
        kBlink,
        kBlinkDown,
        kBlinkUp,
        kBored,
        kDespair,
        kDisoriented,
        kExcited,
        kFocused,
        kFurious,
        kHappy,
        kLookDown,
        kLookLeft,
        kLookRight,
        kLookUp,
        kSad,
        kScared,
        kSleepy,
        kSurprised,
        kWinkLeft,
        kWinkRight,
        kWorried,
    };

    struct EyeParams {
        int16_t x;
        int16_t y;
        int16_t width;
        int16_t height;
        int16_t radius;
        int16_t top_cut;
        int16_t bottom_cut;
        int16_t top_slope_left;
        int16_t top_slope_right;
    };

    struct FaceParams {
        EyeParams left;
        EyeParams right;
    };

    enum class BootPhase : uint8_t { kClosed, kToHalf, kHalfBlink, kToFull, kIdle };
    enum class BlinkState : uint8_t { kIdle, kClosing, kHold, kOpening };
    enum class GazeState : uint8_t { kIdle, kToDirection, kHolding, kToCenter };
    enum class HalfBlinkState : uint8_t { kIdle, kShrinking, kExpanding };
    enum class TransitionState : uint8_t { kIdle, kMorph, kBlinkSwitch };
    enum class Screen : uint8_t {
        kEyes, kBattery, kClock, kWifi, kTemperature, kWeather, kDrive, kFind
    };
    enum class WeatherKind : uint8_t { kUnknown, kClear, kCloudy, kRain, kStorm, kFog, kSnow };

    static void AnimationTimerCallback(lv_timer_t* timer);
    static bool ParseEmotion(const char* emotion, Expression& expression);
    static Expression InferSentenceExpression(const char* content);
    static Screen DetectRequestedScreen(const char* content);
    static WeatherKind DetectWeatherKind(const char* content);
    static bool ParsePercentage(const char* content, int& percentage);
    static bool ParseTemperature(const char* content, float& temperature);
    static float Clamp01(float value);
    static float EaseInOut(float value);
    static int16_t Lerp(int16_t from, int16_t to, float amount);
    static uint32_t NowMs();
    static uint32_t Random(uint32_t minimum, uint32_t maximum);

    int16_t Scale(int16_t value) const;
    FaceParams GetBaseFace() const;
    FaceParams GetExpressionFace(Expression expression) const;
    FaceParams InterpolateFace(const FaceParams& from, const FaceParams& to, float amount) const;
    FaceParams ApplyBlink(const FaceParams& face, float amount, bool half_blink) const;
    FaceParams ComputeFrame(uint32_t now) const;

    void SetRobotEyeExpression(Expression expression, bool blink_switch = false);
    void NotifyEmotionMotion();
    void StartBlink(uint32_t now);
    uint32_t RandomBlinkInterval() const;
    void UpdateBoot(uint32_t now);
    void UpdateBlink(uint32_t now);
    void UpdateHalfBlink(uint32_t now);
    void UpdateGaze(uint32_t now);
    void UpdateDrift(uint32_t now);
    void UpdateTransition(uint32_t now);
    void UpdateAnimation();

    void ShowScreen(Screen screen, uint32_t duration_ms);
    void ShowEyesScreen();
    void RefreshInfoScreen(uint32_t now, bool force = false);
    void UpdateInfoFromAssistant(const char* content);
    void SetInfoText(const char* icon, const char* title, const char* value, const char* detail);
    void RenderWeatherScene(uint32_t now, bool force = false);
    void ClearWeatherCanvas();
    void SetWeatherPixel(int x, int y, bool enabled = true);
    void DrawWeatherBox(int x, int y, int width, int height);
    void DrawWeatherCircle(int center_x, int center_y, int radius);
    void DrawWeatherLine(int x0, int y0, int x1, int y1, int thickness = 1);
    void DrawWeatherCloud(int y_offset = 0);

    void ClearCanvas();
    void SetPixel(int x, int y, bool enabled);
    void DrawBox(int x, int y, int width, int height, bool enabled);
    void DrawRoundedBox(int x, int y, int width, int height, int radius, bool enabled);
    void DrawEye(const EyeParams& eye);
    void DrawFace(const FaceParams& face);
    void DrawExcitedMask(const FaceParams& face, float amount);
    void PresentCanvas();

    lv_obj_t* eyes_canvas_ = nullptr;
    lv_obj_t* info_layer_ = nullptr;
    lv_obj_t* info_icon_label_ = nullptr;
    lv_obj_t* info_title_label_ = nullptr;
    lv_obj_t* info_value_label_ = nullptr;
    lv_obj_t* info_detail_label_ = nullptr;
    lv_obj_t* weather_canvas_ = nullptr;
    lv_draw_buf_t* canvas_draw_buffer_ = nullptr;
    lv_draw_buf_t* weather_draw_buffer_ = nullptr;
    uint8_t* canvas_pixels_ = nullptr;
    uint8_t* weather_pixels_ = nullptr;
    uint32_t canvas_stride_ = 0;
    uint32_t weather_stride_ = 0;
    lv_timer_t* animation_timer_ = nullptr;
    int weather_canvas_width_ = 0;
    int weather_canvas_height_ = 0;

    int geometry_scale_ = 1;
    int16_t base_left_x_ = 0;
    int16_t base_right_x_ = 0;
    int16_t base_y_ = 0;

    BootPhase boot_phase_ = BootPhase::kClosed;
    BlinkState blink_state_ = BlinkState::kIdle;
    GazeState gaze_state_ = GazeState::kIdle;
    HalfBlinkState half_blink_state_ = HalfBlinkState::kIdle;
    TransitionState transition_state_ = TransitionState::kIdle;
    Screen active_screen_ = Screen::kEyes;
    Screen requested_screen_ = Screen::kEyes;
    WeatherKind weather_kind_ = WeatherKind::kUnknown;
    Expression current_expression_ = Expression::kNormal;
    Expression transition_from_ = Expression::kNormal;
    Expression transition_to_ = Expression::kNormal;
    Expression gaze_expression_ = Expression::kNormal;

    uint32_t boot_phase_started_ms_ = 0;
    uint32_t last_boot_blink_ms_ = 0;
    uint32_t blink_started_ms_ = 0;
    uint32_t blink_hold_ms_ = 0;
    uint32_t next_blink_ms_ = 0;
    uint32_t half_blink_started_ms_ = 0;
    uint32_t next_half_blink_ms_ = 0;
    uint32_t gaze_started_ms_ = 0;
    uint32_t gaze_hold_until_ms_ = 0;
    uint32_t next_gaze_ms_ = 0;
    uint32_t next_drift_ms_ = 0;
    uint32_t transition_started_ms_ = 0;
    uint32_t happy_until_ms_ = 0;
    uint32_t screen_until_ms_ = 0;
    uint32_t last_info_refresh_ms_ = 0;
    uint32_t last_weather_frame_ms_ = 0;
    uint32_t last_drive_refresh_ms_ = 0;
    uint32_t last_find_flash_ms_ = 0;
    bool find_flash_bright_ = false;
    int boot_blink_count_ = 0;
    int displayed_battery_level_ = -1;
    float displayed_temperature_ = 0.0f;
    bool displayed_temperature_valid_ = false;

    float drift_x_ = 0.0f;
    float drift_y_ = 0.0f;
    float drift_target_x_ = 0.0f;
    float drift_target_y_ = 0.0f;
    bool speaking_ = false;
    bool server_emotion_received_ = false;
    bool response_sentence_received_ = false;
    bool status_neutral_pending_ = false;
    EmotionMotionCallback emotion_motion_callback_ = nullptr;
};
