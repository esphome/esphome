#include "esp32_dac.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

#ifdef USE_ARDUINO
#include <esp32-hal-dac.h>
#endif

namespace esphome {
namespace esp32_dac {

#ifdef USE_ESP32_VARIANT_ESP32S2
static constexpr uint8_t DAC0_PIN = 17;
#else
static constexpr uint8_t DAC0_PIN = 25;
#endif

static const char *const TAG = "esp32_dac";

void ESP32DAC::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 DAC Output...");
  this->pin_->setup();
  this->turn_off();

#ifdef USE_ESP_IDF
  const dac_channel_t channel = this->pin_->get_pin() == DAC0_PIN ? DAC_CHAN_0 : DAC_CHAN_1;
  const dac_oneshot_config_t oneshot_cfg{channel};
  dac_oneshot_new_channel(&oneshot_cfg, &this->dac_handle_);
#endif
}

void ESP32DAC::on_safe_shutdown() {
#ifdef USE_ESP_IDF
  dac_oneshot_del_channel(this->dac_handle_);
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
  dac_oneshot_output_voltage(this->dac_handle_, state);
#endif
#ifdef USE_ARDUINO
  dacWrite(this->pin_->get_pin(), state);
#endif
}

}  // namespace esp32_dac
}  // namespace esphome

#endif
