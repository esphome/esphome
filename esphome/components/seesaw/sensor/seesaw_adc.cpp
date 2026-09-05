#include "seesaw_adc.h"
#include "esphome/core/log.h"

namespace esphome::seesaw {

static const char *const TAG = "seesaw.adc";

void SeesawADC::setup() { ESP_LOGCONFIG(TAG, "Setting up Seesaw touch sensor..."); }

void SeesawADC::update() {
  uint16_t value = this->parent_->analog_read(this->pin_);
  if (value == 0xffff) {
    ESP_LOGW(TAG, "adc read error");
  } else {
    this->publish_state(value);
  }
}

}  // namespace esphome::seesaw
