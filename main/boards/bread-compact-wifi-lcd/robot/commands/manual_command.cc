#include "manual_command.h"

namespace ruby_robot {

bool ManualCommand::Submit(WheelCommand wheels) {
    if (wheels.left_percent < -100 || wheels.left_percent > 100 ||
        wheels.right_percent < -100 || wheels.right_percent > 100)
        return false;
    wheels_ = wheels;
    return true;
}

}  // namespace ruby_robot
