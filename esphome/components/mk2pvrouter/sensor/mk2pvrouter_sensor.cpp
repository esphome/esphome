#include "esphome/core/log.h"
#include "mk2pvrouter_sensor.h"

namespace esphome {
namespace mk2pvrouter {
static const char *const TAG = "mk2pvrouter_sensor";

Mk2PVRouterSensor::Mk2PVRouterSensor(const char *tag) { this->tag = std::string(tag); }

void Mk2PVRouterSensor::publish_val(const std::string &val) {
  auto result = parse_number<float>(val);
  if (!result.has_value()) {
    ESP_LOGW(TAG, "Failed to parse value '%s' for tag '%s'", val.c_str(), tag.c_str());
    return;  // or publish NaN to indicate invalid data
  }
  publish_state(result.value());
}

void Mk2PVRouterSensor::dump_config() { LOG_SENSOR("  ", "Mk2PVRouter Sensor", this); }
}  // namespace mk2pvrouter
}  // namespace esphome
