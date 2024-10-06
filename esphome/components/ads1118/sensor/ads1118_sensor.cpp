#include "ads1118_sensor.h"

#include "esphome/core/log.h"

namespace esphome {
namespace ads1118 {

static const char *const TAG = "ads1118.sensor";

void ADS1118Sensor::dump_config() {
  LOG_SENSOR("  ", "ADS1118 Sensor", this);
  ESP_LOGCONFIG(TAG, "    Multiplexer: %u", this->multiplexer_);
  ESP_LOGCONFIG(TAG, "    Gain: %u", this->gain_);
}

float ADS1118Sensor::sample() {
  return this->parent_->request_measurement(this->multiplexer_, this->gain_, this->temperature_mode_);
}

void ADS1118Sensor::update() {
  float v = this->sample();
  if (!std::isnan(v)) {
    ESP_LOGD(TAG, "'%s': Got Voltage=%fV", this->get_name().c_str(), v);
    this->publish_state(v);
  }
}

}  // namespace ads1118
}  // namespace esphome
