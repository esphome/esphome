#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::hbridge {

class HBridgeLightOutput : public Component, public light::LightOutput {
 public:
  void set_pina_pin(output::FloatOutput *pina_pin) { this->pina_pin_ = pina_pin; }
  void set_pinb_pin(output::FloatOutput *pinb_pin) { this->pinb_pin_ = pinb_pin; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::COLD_WARM_WHITE});
    traits.set_min_mireds(153);
    traits.set_max_mireds(500);
    return traits;
  }

  void setup() override { this->disable_loop(); }

  void loop() override {
    // Only called when both channels are active — alternate H-bridge direction
    // each iteration to multiplex cold and warm white.
    if (!this->forward_direction_) {
      this->pina_pin_->set_level(this->pina_duty_);
      this->pinb_pin_->set_level(0);
      this->forward_direction_ = true;
    } else {
      this->pina_pin_->set_level(0);
      this->pinb_pin_->set_level(this->pinb_duty_);
      this->forward_direction_ = false;
    }
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void write_state(light::LightState *state) override {
    float new_pina, new_pinb;
    state->current_values_as_cwww(&new_pina, &new_pinb, false);

    this->pina_duty_ = new_pina;
    this->pinb_duty_ = new_pinb;

    if (new_pina != 0.0f && new_pinb != 0.0f) {
      // Both channels active — need loop to alternate H-bridge direction
      this->high_freq_.start();
      this->enable_loop();
    } else {
      // Zero or one channel active — drive pins directly, no multiplexing needed
      this->high_freq_.stop();
      this->disable_loop();
      this->pina_pin_->set_level(new_pina);
      this->pinb_pin_->set_level(new_pinb);
    }
  }

 protected:
  output::FloatOutput *pina_pin_;
  output::FloatOutput *pinb_pin_;
  float pina_duty_{0};
  float pinb_duty_{0};
  bool forward_direction_{false};
  HighFrequencyLoopRequester high_freq_;
};

}  // namespace esphome::hbridge
