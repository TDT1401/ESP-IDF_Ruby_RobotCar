#pragma once

#include <cJSON.h>

#include "../commands/motion_goal_command.h"
#include "../domain/odometry.h"

// Adds owned objects to parent; never reads hardware or mutates controller state.
bool AddOdometryJson(cJSON* parent, const ruby_robot::OdometrySnapshot& pose,
                     const ruby_robot::DistanceDriveSnapshot& command);
