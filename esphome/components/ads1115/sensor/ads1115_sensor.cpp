#include "ads1115_sensor.h"

#include "esphome/core/log.h"

namespace esphome::ads1115 {

ESPHOME_LOG_TAG(TAG, "ads1115.sensor");

float ADS1115Sensor::sample() {
  return this->parent_->request_measurement(this->multiplexer_, this->gain_, this->resolution_, this->samplerate_);
}

void ADS1115Sensor::update() {
  float v = this->sample();
  if (!std::isnan(v)) {
    ESP_LOGD(TAG, "'%s': Got Voltage=%fV", LOG_STR_ARG(this->get_log_name()), v);
    this->publish_state(v);
  }
}

void ADS1115Sensor::dump_config() {
  LOG_SENSOR("  ", "ADS1115 Sensor", this);
  ESP_LOGCONFIG(TAG,
                "    Multiplexer: %u\n"
                "    Gain: %u\n"
                "    Resolution: %u\n"
                "    Sample rate: %u",
                this->multiplexer_, this->gain_, this->resolution_, this->samplerate_);
}

}  // namespace esphome::ads1115
