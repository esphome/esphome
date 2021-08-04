#pragma once

#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace status_led {

static const char *const TAG = "status_led";

class StatusLEDLightOutput : public light::LightOutput, public Component {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supports_brightness(false);
    return traits;
  }

  void loop() override {
    uint32_t new_state = App.get_app_state();

    if (new_state != this->last_app_state_) {
      ESP_LOGD(TAG, "New app state %08X", new_state);
    }

    if ((new_state & STATUS_LED_ERROR) != 0u) {
      this->pin_->digital_write(millis() % 250u < 150u);
      this->last_app_state_ = new_state;
    } else if ((new_state & STATUS_LED_WARNING) != 0u) {
      this->pin_->digital_write(millis() % 1500u < 250u);
      this->last_app_state_ = new_state;
    } else if (new_state != this->last_app_state_) {
      // if no error/warning -> restore light state or turn off
      bool state = false;

      if (lightstate_)
        lightstate_->current_values_as_binary(&state);

      this->pin_->digital_write(state);
      this->last_app_state_ = new_state;

      ESP_LOGD(TAG, "Restoring light state %s", ONOFF(state));
    }
  }

  void setup_state(light::LightState *state) override {
    lightstate_ = state;
    this->pin_->setup();
    ESP_LOGD(TAG, "'%s': Setting initital state", state->get_name().c_str());
    this->write_state(state);
  }

  void write_state(light::LightState *state) override {
    bool binary;
    state->current_values_as_binary(&binary);

    // if in warning/error, don't overwrite the status_led
    // once it is back to OK, the loop will restore the state
    if ((App.get_app_state() & (STATUS_LED_ERROR | STATUS_LED_WARNING)) == 0u) {
      this->pin_->digital_write(binary);
      ESP_LOGD(TAG, "'%s': Setting state %s", state->get_name().c_str(), ONOFF(binary));
    }
  }

  void setup() override {
    ESP_LOGD(TAG, "Setting up Status LED...");

    this->pin_->setup();
    this->pin_->digital_write(false);
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Status Led Light:");
    LOG_PIN("  Pin: ", this->pin_);
    if (this->pin_->is_inverted())
      ESP_LOGCONFIG(TAG, "  Inverted: YES");
  }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  float get_loop_priority() const override { return 50.0f; }

 protected:
  GPIOPin *pin_;
  light::LightState *lightstate_{};
  uint32_t last_app_state_{0xFFFF};
};

}  // namespace status_led
}  // namespace esphome
