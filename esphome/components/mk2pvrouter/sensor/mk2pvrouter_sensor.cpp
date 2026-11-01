#include "mk2pvrouter_sensor.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::mk2pvrouter {

ESPHOME_LOG_TAG(TAG, "mk2pvrouter_sensor");

void publish_sensor_value(sensor::Sensor *sensor, const char *own_tag, bool scale_centi, const char *tag,
                          const char *val) {
  if (strcmp(tag, own_tag) != 0)
    return;
  auto result = parse_number<float>(val);
  if (!result.has_value()) {
    ESP_LOGW(TAG, "Failed to parse value '%s' for tag '%s'", val, own_tag);
    return;
  }
  float value = result.value();
  if (scale_centi) {
    value *= 0.01f;
  }
  sensor->publish_state(value);
}

void dump_sensor(sensor::Sensor *sensor, const char *own_tag) {
  LOG_SENSOR("  ", "Mk2PVRouter Sensor", sensor);
  ESP_LOGCONFIG(TAG, "  Tag: %s", own_tag);
}

}  // namespace esphome::mk2pvrouter
