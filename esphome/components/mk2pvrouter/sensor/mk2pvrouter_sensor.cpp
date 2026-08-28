#include "mk2pvrouter_sensor.h"
#include "esphome/core/log.h"

namespace esphome::mk2pvrouter {

static const char *const TAG = "mk2pvrouter_sensor";

Mk2PVRouterSensor::Mk2PVRouterSensor(const char *tag) : Mk2PVRouterListener(tag) {}

void Mk2PVRouterSensor::publish_val(const char *val) {
  auto result = parse_number<float>(val);
  if (!result.has_value()) {
    ESP_LOGW(TAG, "Failed to parse value '%s' for tag '%s'", val, this->get_tag());
    return;
  }
  float value = result.value();
  // Voltage (V, V1, V2, ...) and temperature (T1, T2, ...) tags are sent by the
  // device multiplied by 100 (centivolts/centi-degrees); undo that here so
  // downstream filters see the value already in volts/°C.
  const char tag0 = this->get_tag()[0];
  if (tag0 == 'V' || tag0 == 'T') {
    value *= 0.01f;
  }
  this->publish_state(value);
}

void Mk2PVRouterSensor::dump_config() {
  LOG_SENSOR("  ", "Mk2PVRouter Sensor", this);
  ESP_LOGCONFIG(TAG, "  Tag: %s", this->get_tag());
}

}  // namespace esphome::mk2pvrouter
