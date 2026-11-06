#pragma once

#include <cJSON.h>

#include "../commands/distance_drive_command.h"
#include "../domain/odometry.h"

// Adds owned objects to parent; never reads hardware or mutates controller state.
bool AddOdometryJson(cJSON* parent, const ruby_robot::OdometrySnapshot& pose,
                     const ruby_robot::DistanceDriveSnapshot& command);
