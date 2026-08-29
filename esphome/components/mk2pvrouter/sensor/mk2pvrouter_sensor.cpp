#include "mk2pvrouter_sensor.h"
#include <cctype>
#include "esphome/core/log.h"

namespace esphome::mk2pvrouter {

static const char *const TAG = "mk2pvrouter_sensor";

Mk2PVRouterSensor::Mk2PVRouterSensor(const char *tag) : Mk2PVRouterListener(tag) {
  // Voltage (V, V1, V2, ...) and temperature (T1, T2, ...) tags are sent by the
  // device multiplied by 100 (centivolts/centi-degrees); undo that here so
  // downstream filters see the value already in volts/°C. This mirrors the
  // exact/pattern matching used by apply_tag_defaults() in sensor/__init__.py:
  // "V" alone or "V"/"T" followed solely by digits are scaled; bare "T" never
  // occurs on the wire and has no exact-match entry, so it is excluded here too.
  const char tag0 = tag[0];
  if (tag0 == 'V' || tag0 == 'T') {
    bool matches = true;
    for (const char *c = tag + 1; *c != '\0'; c++) {
      if (!std::isdigit(static_cast<unsigned char>(*c))) {
        matches = false;
        break;
      }
    }
    if (matches && tag0 == 'T' && tag[1] == '\0') {
      matches = false;
    }
    this->scale_centi_ = matches;
  }
}

void Mk2PVRouterSensor::publish_val(const char *val) {
  auto result = parse_number<float>(val);
  if (!result.has_value()) {
    ESP_LOGW(TAG, "Failed to parse value '%s' for tag '%s'", val, this->get_tag());
    return;
  }
  float value = result.value();
  if (this->scale_centi_) {
    value *= 0.01f;
  }
  this->publish_state(value);
}

void Mk2PVRouterSensor::dump_config() {
  LOG_SENSOR("  ", "Mk2PVRouter Sensor", this);
  ESP_LOGCONFIG(TAG, "  Tag: %s", this->get_tag());
  if (this->scale_centi_) {
    ESP_LOGCONFIG(TAG, "  Scale: x0.01");
  }
}

}  // namespace esphome::mk2pvrouter
