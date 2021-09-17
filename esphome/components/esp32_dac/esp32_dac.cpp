#include "esp32_dac.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

#ifdef USE_ARDUINO
#include <esp32-hal-dac.h>
#endif
#ifdef USE_ESP_IDF
#include <driver/dac.h>
#endif

namespace esphome {
namespace esp32_dac {

static const char *const TAG = "esp32_dac";

void ESP32DAC::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 DAC Output...");
  this->pin_->setup();
  this->turn_off();

#ifdef USE_ESP_IDF
  auto channel = pin_->get_pin() == 25 ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
  dac_output_enable(channel);
#endif
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

#ifdef USE_ESP_IDF
  auto channel = pin_->get_pin() == 25 ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
  dac_output_voltage(channel, (uint8_t) state);
#endif
#ifdef USE_ARDUINO
  dacWrite(this->pin_->get_pin(), state);
#endif
}

}  // namespace esp32_dac
}  // namespace esphome

#endif
