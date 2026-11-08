#pragma once

#include <cJSON.h>

#include "../commands/motion_goal_command.h"
#include "../domain/odometry.h"
#include "../domain/navigator.h"
#include "../domain/pose_ekf.h"

// Adds owned objects to parent; never reads hardware or mutates controller state.
bool AddOdometryJson(cJSON* parent, const ruby_robot::OdometrySnapshot& pose,
                     const ruby_robot::EkfSnapshot& ekf,
                     const ruby_robot::DistanceDriveSnapshot& command,
                     const ruby_robot::NavigateSnapshot& navigation);
