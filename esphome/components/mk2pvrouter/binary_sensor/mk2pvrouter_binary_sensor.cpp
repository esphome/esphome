#include "mk2pvrouter_binary_sensor.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::mk2pvrouter {

static const char *const TAG = "mk2pvrouter_binary_sensor";

Mk2PVRouterBinarySensor::Mk2PVRouterBinarySensor(const char *tag) : Mk2PVRouterListener(tag) {}

void Mk2PVRouterBinarySensor::publish_val(const char *val) {
  bool state = (strcmp(val, "0") != 0);
  this->publish_state(state);
}

void Mk2PVRouterBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("  ", "Mk2PVRouter Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  Tag: %s", this->get_tag());
}

}  // namespace esphome::mk2pvrouter
