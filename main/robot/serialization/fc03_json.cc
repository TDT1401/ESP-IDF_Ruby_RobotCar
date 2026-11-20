#include "fc03_json.h"

namespace {

bool AddMetric(cJSON* json, const char* key, double value, bool valid) {
    return valid ? cJSON_AddNumberToObject(json, key, value) != nullptr
                 : cJSON_AddNullToObject(json, key) != nullptr;
}

bool AddWheel(cJSON* parent, const char* name, const Fc03WheelStatus& wheel, bool valid) {
    auto* json = cJSON_AddObjectToObject(parent, name);
    return json && cJSON_AddNumberToObject(json, "pulses", wheel.pulses) &&
           cJSON_AddNumberToObject(json, "unknown_direction_pulses",
                                   wheel.unknown_direction_pulses) &&
           cJSON_AddNumberToObject(json, "hz", wheel.hz) &&
           cJSON_AddNumberToObject(json, "level", wheel.level) &&
           cJSON_AddNumberToObject(json, "command", wheel.command) &&
           cJSON_AddBoolToObject(json, "no_pulses", wheel.no_pulses) &&
           AddMetric(json, "rpm", wheel.rpm, valid) &&
           AddMetric(json, "distance_m", wheel.distance_m, valid) &&
           AddMetric(json, "signed_distance_m", wheel.signed_distance_m, valid);
}

}  // namespace

bool AddFc03EncoderJson(cJSON* parent, const Fc03EncoderStatus& status) {
    auto* json = cJSON_AddObjectToObject(parent, "encoder");
    const bool valid = status.ready && status.calibrated;
    return json && cJSON_AddBoolToObject(json, "enabled", status.enabled) &&
           cJSON_AddBoolToObject(json, "ready", status.ready) &&
           cJSON_AddBoolToObject(json, "calibrated", status.calibrated) &&
           cJSON_AddBoolToObject(json, "distance_complete", status.distance_complete) &&
           cJSON_AddStringToObject(json, "direction_source", "motor_command") &&
           cJSON_AddStringToObject(json, "error", esp_err_to_name(status.error)) &&
           AddMetric(json, "travel_m", status.travel_m, valid) &&
           AddWheel(json, "left", status.left, valid) &&
           AddWheel(json, "right", status.right, valid);
}
