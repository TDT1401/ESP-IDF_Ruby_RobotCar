#pragma once

#include "wheel_command.h"

namespace ruby_robot {

// Holds the most recent wheel targets for an active manual session.
class ManualCommand {
public:
    bool Submit(WheelCommand wheels);
    void Clear() { wheels_ = {}; }
    WheelCommand wheels() const { return wheels_; }

private:
    WheelCommand wheels_;
};

}  // namespace ruby_robot
