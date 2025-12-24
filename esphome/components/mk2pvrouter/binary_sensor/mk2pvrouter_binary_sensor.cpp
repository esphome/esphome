#include "esphome/core/log.h"
#include "mk2pvrouter_binary_sensor.h"

namespace esphome::mk2pvrouter {

static const char *const TAG = "mk2pvrouter_binary_sensor";

Mk2PVRouterBinarySensor::Mk2PVRouterBinarySensor(const char *tag) { this->set_tag(tag); }

void Mk2PVRouterBinarySensor::publish_val(const std::string &val) {
  // Convert the string value to a boolean (e.g., "1" -> true, "0" -> false)
  bool state = (val != "0");
  this->publish_state(state);
}

void Mk2PVRouterBinarySensor::dump_config() { LOG_BINARY_SENSOR("  ", "Mk2PVRouter Binary Sensor", this); }

}  // namespace esphome::mk2pvrouter
