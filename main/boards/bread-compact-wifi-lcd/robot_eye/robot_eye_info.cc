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

#include <sdkconfig.h>

#include "board.h"
#include "material_symbols.h"
#include "robot_eye_constants.h"
#include "wifi_manager.h"

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_L298N
#include "../robot/controller/l298n_controller.h"
#endif

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
    const auto match = std::search(
        text_view.begin(), text_view.end(), phrase.begin(), phrase.end(), [](char a, char b) {
            const auto lower = [](unsigned char character) {
                return character >= 'A' && character <= 'Z' ? character + 32 : character;
            };
            return lower(a) == lower(b);
        });
    if (match == text_view.end()) {
        return nullptr;
    }
    return text + (match - text_view.begin()) + phrase.size();
}
}  // namespace

RobotEyeDisplay::Screen RobotEyeDisplay::DetectRequestedScreen(const char* content) {
    if (content == nullptr || content[0] == '\0') {
        return Screen::kEyes;
    }
    const std::string_view text(content);
    for (auto phrase : {"mấy giờ", "giờ hiện tại", "xem giờ", "what time", "current time"}) {
        if (ContainsPhrase(text, phrase)) {
            return Screen::kClock;
        }
    }
    for (auto phrase :
         {"wi-fi", "wifi", "trạng thái mạng", "kết nối mạng", "địa chỉ ip", "internet"}) {
        if (ContainsPhrase(text, phrase)) {
            return Screen::kWifi;
        }
    }
    for (auto phrase : {"thời tiết", "dự báo", "hôm nay trời", "trời hôm nay", "trời có", "weather",
                        "forecast"}) {
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
    const bool explicitly_dry =
        ContainsPhrase(text, "không mưa") || ContainsPhrase(text, "không có mưa") ||
        ContainsPhrase(text, "no rain") || ContainsPhrase(text, "no rainfall") ||
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
            StartsWithPhrase(suffix_text, "degrees") || StartsWithPhrase(suffix_text, "celsius") ||
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
               !(*value_start == '-' && std::isdigit(static_cast<unsigned char>(value_start[1])))) {
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

void RobotEyeDisplay::RefreshInfoScreen(uint32_t now, bool force) {
    if (active_screen_ == Screen::kEyes ||
        (!force && now - last_info_refresh_ms_ < kInfoRefreshPeriodMs)) {
        return;
    }
    last_info_refresh_ms_ = now;
    char value[64] = {};
    char detail[256] = {};
    switch (active_screen_) {
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
                if (std::fabs(displayed_temperature_ - std::round(displayed_temperature_)) <
                    0.05f) {
                    snprintf(value, sizeof(value),
                             "%.0f\xC2\xB0"
                             "C",
                             displayed_temperature_);
                } else {
                    snprintf(value, sizeof(value),
                             "%.1f\xC2\xB0"
                             "C",
                             displayed_temperature_);
                }
            } else {
                snprintf(value, sizeof(value),
                         "--\xC2\xB0"
                         "C");
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
                    weather_label =
                        displayed_temperature_valid_ ? "CURRENT WEATHER" : "WAITING FOR WEATHER";
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
            const char* owner = strcmp(motor.control_owner, "web") == 0        ? "WEB"
                                : strcmp(motor.control_owner, "telegram") == 0 ? "TELEGRAM"
                                : strcmp(motor.control_owner, "voice") == 0    ? "VOICE"
                                                                               : "AUTO";
            char range[20];
            if (motor.distance_valid)
                snprintf(range, sizeof(range), "%dcm", motor.measured_distance_cm);
            else
                snprintf(range, sizeof(range), "--");
            const bool warning =
                motor.distance_valid &&
                motor.measured_distance_cm < (motor.web_manual ? motor.web_warning_effective_cm
                                                               : motor.obstacle_stop_effective_cm);
            snprintf(detail, sizeof(detail), "SR04 %s  |  %s\nBEEP %s%s", range, owner,
                     motor.web_manual ? (motor.warning_sound_enabled ? "ON" : "OFF") : "--",
                     warning ? "  ! NEAR" : "");
            SetInfoText(MATERIAL_SYMBOLS_WIFI, "RUBY DRIVE", value, detail);
#endif
            break;
        }
        case Screen::kEyes:
            break;
    }
}

void RobotEyeDisplay::UpdateInfoFromAssistant(const char* content) {
    if (requested_screen_ == Screen::kTemperature || requested_screen_ == Screen::kWeather) {
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
