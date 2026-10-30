#include "mk2pvrouter_binary_sensor.h"
#include <cstring>

namespace esphome::mk2pvrouter {

Mk2PVRouterBinarySensor::Mk2PVRouterBinarySensor(const char *tag) : Mk2PVRouterListener(tag) {}

void Mk2PVRouterBinarySensor::publish_val(const char *val) {
  bool state = (strcmp(val, "0") != 0);
  this->publish_state(state);
}

}  // namespace esphome::mk2pvrouter
