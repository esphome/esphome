#include "template_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.binary_sensor";

void TemplateBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Template Binary Sensor", this); }

}  // namespace esphome::template_
