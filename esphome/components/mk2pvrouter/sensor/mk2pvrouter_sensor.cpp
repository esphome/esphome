#include "esphome/core/log.h"
#include "mk2pvrouter_sensor.h"

namespace esphome {
namespace mk2pvrouter {
static const char *const TAG = "mk2pvrouter_sensor";

Mk2PVRouterSensor::Mk2PVRouterSensor(const char *tag) { this->tag = std::string(tag); }

void Mk2PVRouterSensor::publish_val(const std::string &val) {
  auto newval = parse_number<float>(val).value_or(0.0f);
  publish_state(newval);
}

void Mk2PVRouterSensor::dump_config() { LOG_SENSOR("  ", "Mk2PVRouter Sensor", this); }
}  // namespace mk2pvrouter
}  // namespace esphome
