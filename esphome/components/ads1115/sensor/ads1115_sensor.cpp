#include "ads1115_sensor.h"

#include "esphome/core/log.h"

namespace esphome {
namespace ads1115 {

static const char *const TAG = "ads1115.sensor";

float ADS1115Sensor::sample() {
  return this->parent_->request_measurement(this->multiplexer_, this->gain_, this->resolution_);
}

void ADS1115Sensor::update() {
  float v = this->sample();
  if (!std::isnan(v)) {
    ESP_LOGD(TAG, "'%s': Got Voltage=%fV", this->get_name().c_str(), v);
    this->publish_state(v);
  }
}

void ADS1115Sensor::dump_config() {
  LOG_SENSOR("  ", "ADS1115 Sensor", this);
  ESP_LOGCONFIG(TAG, "    Multiplexer: %u", this->multiplexer_);
  ESP_LOGCONFIG(TAG, "    Gain: %u", this->gain_);
  ESP_LOGCONFIG(TAG, "    Resolution: %u", this->resolution_);
}

}  // namespace ads1115
}  // namespace esphome
