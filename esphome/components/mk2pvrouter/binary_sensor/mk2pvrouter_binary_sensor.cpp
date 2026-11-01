#include "mk2pvrouter_binary_sensor.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::mk2pvrouter {

void publish_binary_sensor_value(binary_sensor::BinarySensor *sensor, const char *own_tag, const char *tag,
                                 const char *val) {
  if (strcmp(tag, own_tag) != 0)
    return;
  sensor->publish_state(strcmp(val, "0") != 0);
}

void dump_binary_sensor(const char *tag, binary_sensor::BinarySensor *sensor, const char *own_tag) {
  binary_sensor::log_binary_sensor(tag, "  ", LOG_STR_LITERAL("Mk2PVRouter Binary Sensor"), sensor);
  ESP_LOGCONFIG(tag, "  Tag: %s", own_tag);
}

}  // namespace esphome::mk2pvrouter
