#include "resistance_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace resistance {

static const char *const TAG = "resistance";

void ResistanceSensor::dump_config() {
  LOG_SENSOR("", "Resistance Sensor", this);
  ESP_LOGCONFIG(TAG, "  Configuration: %s", this->configuration_ == UPSTREAM ? "UPSTREAM" : "DOWNSTREAM");
  ESP_LOGCONFIG(TAG, "  Resistor: %.2fΩ", this->resistor_);
  ESP_LOGCONFIG(TAG, "  Reference Voltage: %.1fV", this->reference_voltage_);
}
void ResistanceSensor::process_(float value) {
  if (std::isnan(value)) {
    this->publish_state(NAN);
    return;
  }
  float res = 0;
  switch (this->configuration_) {
    case UPSTREAM:
      if (value == 0.0f)
        res = NAN;
      else
        res = (this->reference_voltage_ - value) / value;
      break;
    case DOWNSTREAM:
      if (value == this->reference_voltage_)
        res = NAN;
      else
        res = value / (this->reference_voltage_ - value);
      break;
  }

  res *= this->resistor_;
  ESP_LOGD(TAG, "'%s' - Resistance %.1fΩ", this->name_.c_str(), res);
  this->publish_state(res);
}

}  // namespace resistance
}  // namespace esphome
