#include "esphome/core/log.h"
#include "mk2pvrouter_binary_sensor.h"

namespace esphome {
namespace mk2pvrouter {
static const char *const TAG = "mk2pvrouter_binary_sensor";

Mk2PVRouterBinarySensor::Mk2PVRouterBinarySensor(const char *tag) { this->tag = std::string(tag); }

void Mk2PVRouterBinarySensor::publish_val(const std::string &val) {
  // Convert the string value to a boolean (e.g., "1" -> true, "0" -> false)
  bool state = (val != "0");
  publish_state(state);
}

void Mk2PVRouterBinarySensor::dump_config() { LOG_BINARY_SENSOR("  ", "Mk2PVRouter Binary Sensor", this); }
}  // namespace mk2pvrouter
}  // namespace esphome
