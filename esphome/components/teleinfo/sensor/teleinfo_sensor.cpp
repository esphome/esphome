#include "esphome/core/log.h"
#include "teleinfo_sensor.h"
namespace esphome {
namespace teleinfo {

static const char *TAG = "teleinfo_sensor";
TeleInfoSensor::TeleInfoSensor(const char *tag) { this->tag = std::string(tag); }
void TeleInfoSensor::publish_val(std::string val) {
  auto newval = parse_float(val);
  publish_state(*newval);
}
void TeleInfoSensor::dump_config() { LOG_SENSOR("  ", tag.c_str(), this); }
}  // namespace teleinfo
}  // namespace esphome
