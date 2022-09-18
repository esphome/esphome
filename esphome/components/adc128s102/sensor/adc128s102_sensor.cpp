#include "adc128s102_sensor.h"

#include "esphome/core/log.h"

namespace esphome {
namespace adc128s102 {

static const char *const TAG = "adc128s102.sensor";

ADC128S102Sensor::ADC128S102Sensor(uint8_t channel) : channel_(channel) {}

float ADC128S102Sensor::get_setup_priority() const { return setup_priority::DATA; }

void ADC128S102Sensor::dump_config() {
  LOG_SENSOR("", "ADC128S102 Sensor", this);
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->channel_);
  LOG_UPDATE_INTERVAL(this);
}

float ADC128S102Sensor::sample() { return this->parent_->read_data(this->channel_); }
void ADC128S102Sensor::update() { this->publish_state(this->sample()); }

}  // namespace adc128s102
}  // namespace esphome
