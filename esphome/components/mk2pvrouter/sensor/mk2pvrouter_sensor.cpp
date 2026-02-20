#include "esphome/core/log.h"
#include "mk2pvrouter_sensor.h"

namespace esphome::mk2pvrouter {

static const char *const TAG = "mk2pvrouter_sensor";

Mk2PVRouterSensor::Mk2PVRouterSensor(const char *tag) { this->set_tag(tag); }

void Mk2PVRouterSensor::publish_val(const char *val) {
  auto result = parse_number<float>(val);
  if (!result.has_value()) {
    ESP_LOGW(TAG, "Failed to parse value '%s' for tag '%s'", val, this->get_tag());
    return;
  }
  this->publish_state(result.value());
}

void Mk2PVRouterSensor::dump_config() { LOG_SENSOR("  ", "Mk2PVRouter Sensor", this); }

}  // namespace esphome::mk2pvrouter
