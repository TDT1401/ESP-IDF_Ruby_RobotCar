#include "robot_board.h"

#include <cstring>

#include "../robot_eye/robot_eye_display.h"
#include "controller/l298n_controller.h"
#include "mcp/mcp_tools.h"
#include "telegram/telegram.h"
#include "web/web_server.h"

namespace {

RobotDriveStatus ReadRobotDriveStatus() {
    const auto motor = GetL298nMotorStatus();
    RobotDriveStatus view;
    view.active = motor.web_manual || motor.pending || motor.moving ||
                  strcmp(motor.autonomy_mode, "manual") != 0;
    view.motion = motor.motion;
    view.speed_percent = motor.active_speed_percent;
    view.owner = motor.control_owner;
    view.web_manual = motor.web_manual;
    view.distance_valid = motor.distance_valid;
    view.distance_cm = motor.measured_distance_cm;
    view.warning_distance_cm = motor.web_manual ? motor.web_warning_effective_cm
                                                 : motor.obstacle_stop_effective_cm;
    view.warning_sound_enabled = motor.warning_sound_enabled;
    return view;
}

}  // namespace

void InitializeRobotBoard(RobotEyeDisplay& display) {
    InitializeL298nController();
    RegisterL298nMcpTools();
    InitializeL298nTelegram(IsL298nWebServerRunning);
    display.SetEmotionMotionCallback(UpdateL298nEmotion);
    display.SetDriveStatusProvider(ReadRobotDriveStatus);
}

void UpdateRobotNetwork(bool connected) {
    UpdateL298nWebNetwork(connected);
    UpdateL298nTelegramNetwork(connected);
}
