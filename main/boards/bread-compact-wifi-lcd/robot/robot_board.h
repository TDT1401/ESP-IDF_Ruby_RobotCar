#pragma once

class RobotEyeDisplay;

// Board composition root for the optional RobotCar subsystem.
void InitializeRobotBoard(RobotEyeDisplay& display);
void UpdateRobotNetwork(bool connected);
