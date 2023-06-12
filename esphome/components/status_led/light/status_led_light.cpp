#include "status_led_light.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace status_led {

static const char *const TAG = "status_led";

void StatusLEDLightOutput::loop() {
  uint32_t new_state = App.get_app_state() & STATUS_LED_MASK;

  if (new_state != this->last_app_state_) {
    ESP_LOGV(TAG, "New app state 0x%08X", new_state);
  }

  if ((new_state & STATUS_LED_ERROR) != 0u) {
    this->output_state_(millis() % 250u < 150u);
    this->last_app_state_ = new_state;
  } else if ((new_state & STATUS_LED_WARNING) != 0u) {
    this->output_state_(millis() % 1500u < 250u);
    this->last_app_state_ = new_state;
  } else if (new_state != this->last_app_state_) {
    // if no error/warning -> restore light state or turn off
    bool state = false;

    if (lightstate_)
      lightstate_->current_values_as_binary(&state);
    ESP_LOGD(TAG, "Restoring light state %s", ONOFF(state));

    this->output_state_(state);
    this->last_app_state_ = new_state;
  }
}

void StatusLEDLightOutput::setup_state(light::LightState *state) {
  lightstate_ = state;
  ESP_LOGD(TAG, "'%s': Setting initial state", state->get_name().c_str());
  this->write_state(state);
}

void StatusLEDLightOutput::write_state(light::LightState *state) {
  bool binary;
  state->current_values_as_binary(&binary);

  // if in warning/error, don't overwrite the status_led
  // once it is back to OK, the loop will restore the state
  if ((App.get_app_state() & (STATUS_LED_ERROR | STATUS_LED_WARNING)) == 0u) {
    ESP_LOGD(TAG, "'%s': Setting state %s", state->get_name().c_str(), ONOFF(binary));
    this->output_state_(binary);
  }
}

void StatusLEDLightOutput::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Status LED...");

  if (this->pin_ != nullptr) {
    this->pin_->setup();
    this->pin_->digital_write(false);
  }
}

void StatusLEDLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Status Led Light:");
  LOG_PIN("  Pin: ", this->pin_);
}

void StatusLEDLightOutput::output_state_(bool state) {
  if (this->pin_ != nullptr)
    this->pin_->digital_write(state);
  if (this->output_ != nullptr)
    this->output_->set_state(state);
}

}  // namespace status_led
}  // namespace esphome
