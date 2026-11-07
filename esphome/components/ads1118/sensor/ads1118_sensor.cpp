#include "ads1118_sensor.h"

#include "esphome/core/log.h"

namespace esphome::ads1118 {

ESPHOME_LOG_TAG(TAG, "ads1118.sensor");

void ADS1118Sensor::dump_config() {
  LOG_SENSOR("  ", "ADS1118 Sensor", this);
  ESP_LOGCONFIG(TAG, "    Config: 0x%04X", this->config_bits_);
}

float ADS1118Sensor::sample() { return this->parent_->request_measurement(this->config_bits_); }

void ADS1118Sensor::update() {
  float v = this->sample();
  if (!std::isnan(v)) {
    ESP_LOGD(TAG, "'%s': Got Voltage=%fV", LOG_STR_ARG(this->get_log_name()), v);
    this->publish_state(v);
  }
}

}  // namespace esphome::ads1118
