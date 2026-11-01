#pragma once

#include <cJSON.h>

#include "../sensors/fc03_encoder.h"

// Adds one owned "encoder" object. Distances/RPM are JSON null until calibrated/ready.
bool AddFc03EncoderJson(cJSON* parent, const Fc03EncoderStatus& status);
