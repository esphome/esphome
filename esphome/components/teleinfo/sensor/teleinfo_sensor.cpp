#include "esphome/core/log.h"
#include "teleinfo_sensor.h"
namespace esphome {
namespace teleinfo {

static const char *const TAG = "teleinfo_sensor";
void TeleInfoSensor::publish_val(const char *val) {
  auto newval = parse_number<float>(val).value_or(0.0f);
  publish_state(newval);
}
void TeleInfoSensor::dump_config() { LOG_SENSOR("  ", "Teleinfo Sensor", this); }
}  // namespace teleinfo
}  // namespace esphome
