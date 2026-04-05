#include "template_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.binary_sensor";

void log_template_binary_sensor(binary_sensor::BinarySensor *obj) {
  LOG_BINARY_SENSOR("", "Template Binary Sensor", obj);
}

}  // namespace esphome::template_
