#include "seesaw_adc.h"
#include "esphome/core/log.h"

namespace esphome::seesaw {

ESPHOME_LOG_TAG(TAG, "seesaw.adc");

void SeesawADC::dump_config() {
  LOG_SENSOR("", "Seesaw ADC", this);
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
  LOG_UPDATE_INTERVAL(this);
}

void SeesawADC::update() {
  uint16_t value = this->parent_->analog_read(this->pin_);
  if (value == 0xffff) {
    ESP_LOGW(TAG, "adc read error");
  } else {
    this->publish_state(value);
  }
}

}  // namespace esphome::seesaw
