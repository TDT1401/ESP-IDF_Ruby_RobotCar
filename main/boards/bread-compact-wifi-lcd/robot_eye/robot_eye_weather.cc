#include "robot_eye_display.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "robot_eye_constants.h"

using namespace robot_eye_detail;

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
        for (int column = std::max(0, x); column < std::min(weather_canvas_width_, x + width);
             ++column) {
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
            DrawWeatherLine(x - unit / 15, top + unit / 5, x + unit / 18, top + unit * 38 / 100, 2);
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
