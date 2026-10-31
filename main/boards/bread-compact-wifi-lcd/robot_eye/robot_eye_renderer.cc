#include "robot_eye_display.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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
        const int start_x =
            eye.top_slope_right > eye.top_slope_left ? eye.x + eye.width - black_width : eye.x;
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
            const float normalized =
                static_cast<float>(row * row) / static_cast<float>(radius_y * radius_y);
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
