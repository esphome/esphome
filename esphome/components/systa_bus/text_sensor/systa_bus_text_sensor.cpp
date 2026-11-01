#include "systa_bus_text_sensor.h"
#include "esphome/core/log.h"

#include <cstdio>

namespace esphome::systa_bus {

void publish_aqua_error_code(text_sensor::TextSensor *sensor, std::span<const uint8_t> message) {
  if (message[15] == 0) {
    sensor->publish_state("--");
    return;
  }
  char buf[4];
  int len = snprintf(buf, sizeof(buf), "%02d", message[15]);
  sensor->publish_state(buf, static_cast<size_t>(len));
}

void dump_aqua_error_code(const char *tag, text_sensor::TextSensor *sensor) {
  text_sensor::log_text_sensor(tag, "  ", LOG_STR_LITERAL("Error code"), sensor);
}

}  // namespace esphome::systa_bus
