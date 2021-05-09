#include "esp32_dac.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp32-hal-dac.h>

namespace esphome {
namespace esp32_dac {

static const char *TAG = "esp32_dac";

void ESP32DAC::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 DAC Output...");
  this->pin_->setup();
  this->turn_off();
}

void ESP32DAC::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 DAC:");
  LOG_PIN("  Pin: ", this->pin_);
  LOG_FLOAT_OUTPUT(this);
}

void ESP32DAC::write_state(float state) {
  if (this->pin_->is_inverted())
    state = 1.0f - state;

  state = state * 255;
  dacWrite(this->pin_->get_pin(), state);
}

}  // namespace esp32_dac
}  // namespace esphome

#endif
