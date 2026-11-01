#include "mk2pvrouter_text_sensor.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::mk2pvrouter {

ESPHOME_LOG_TAG(TAG, "mk2pvrouter_text_sensor");

void publish_text_sensor_value(text_sensor::TextSensor *sensor, const char *own_tag, const char *tag, const char *val) {
  if (strcmp(tag, own_tag) != 0)
    return;
  sensor->publish_state(val);
}

void dump_text_sensor(text_sensor::TextSensor *sensor, const char *own_tag) {
  LOG_TEXT_SENSOR("  ", "Mk2PVRouter Text Sensor", sensor);
  ESP_LOGCONFIG(TAG, "  Tag: %s", own_tag);
}

}  // namespace esphome::mk2pvrouter
