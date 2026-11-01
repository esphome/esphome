#include "mk2pvrouter_binary_sensor.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::mk2pvrouter {

ESPHOME_LOG_TAG(TAG, "mk2pvrouter_binary_sensor");

void publish_binary_sensor_value(binary_sensor::BinarySensor *sensor, const char *own_tag, const char *tag,
                                 const char *val) {
  if (strcmp(tag, own_tag) != 0)
    return;
  sensor->publish_state(strcmp(val, "0") != 0);
}

void dump_binary_sensor(binary_sensor::BinarySensor *sensor, const char *own_tag) {
  LOG_BINARY_SENSOR("  ", "Mk2PVRouter Binary Sensor", sensor);
  ESP_LOGCONFIG(TAG, "  Tag: %s", own_tag);
}

}  // namespace esphome::mk2pvrouter
