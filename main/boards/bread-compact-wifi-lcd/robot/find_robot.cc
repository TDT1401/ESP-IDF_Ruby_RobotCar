#include "find_robot.h"

#include <esp_timer.h>

#include <atomic>
#include <cstdint>
#include <cstring>

#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "controller/l298n_controller.h"

#if defined(CONFIG_BREAD_COMPACT_WIFI_LCD_ST7735_80X160) || defined(CONFIG_LCD_ST7789_240X240)
#include "../robot_eye_display.h"
#endif

namespace {
constexpr uint32_t kFindCooldownMs = 8000;
std::atomic<uint32_t> last_find_ms{0};
}  // namespace

esp_err_t FindL298nRobot() {
    const auto motor = GetL298nMotorStatus();
    if (motor.web_manual || motor.pending || motor.moving ||
        strcmp(motor.autonomy_mode, "manual") != 0 ||
        strcmp(motor.route_state, "running") == 0 ||
        strcmp(motor.scan_state, "scanning") == 0)
        return ESP_ERR_INVALID_STATE;
    if (!Application::GetInstance().CanEnterSleepMode())
        return ESP_ERR_INVALID_STATE;

    const uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    uint32_t last = last_find_ms.load();
    do {
        if (last != 0 && now - last < kFindCooldownMs)
            return ESP_ERR_INVALID_STATE;
    } while (!last_find_ms.compare_exchange_weak(last, now));

    Application::GetInstance().Schedule([]() {
        auto& app = Application::GetInstance();
#if defined(CONFIG_BREAD_COMPACT_WIFI_LCD_ST7735_80X160) || defined(CONFIG_LCD_ST7789_240X240)
        if (auto* display = Board::GetInstance().GetDisplay())
            static_cast<RobotEyeDisplay*>(display)->ShowFindFor(6000);
#endif
        if (app.CanEnterSleepMode())
            app.PlaySound(Lang::Sounds::OGG_POPUP);
    });
    return ESP_OK;
}
