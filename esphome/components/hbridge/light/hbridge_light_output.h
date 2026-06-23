#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"

namespace esphome::hbridge {

class HBridgeLightOutput final : public PollingComponent, public light::LightOutput {
 public:
  HBridgeLightOutput() : PollingComponent(SWITCH_INTERVAL_MS) {}

  void set_pina_pin(output::FloatOutput *pina_pin) { this->pina_pin_ = pina_pin; }
  void set_pinb_pin(output::FloatOutput *pinb_pin) { this->pinb_pin_ = pinb_pin; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::COLD_WARM_WHITE});
    traits.set_min_mireds(153);
    traits.set_max_mireds(500);
    return traits;
  }

  // Start with the poller disabled; it is only run while multiplexing (see write_state).
  void setup() override { this->stop_poller(); }

  void update() override {
    // Only polled when both channels are active. Alternate the H-bridge direction every
    // SWITCH_INTERVAL_MS to multiplex cold and warm white. Holding each direction for a
    // fixed interval (rather than flipping every loop iteration) keeps it close to the
    // output's PWM carrier period, so a channel is not switched away mid-cycle and
    // collapsed onto one side. 1 ms matches the cadence used before the loop()-based
    // rewrite regressed this.
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
      // Both channels active — multiplex the H-bridge direction via the poller.
      if (!this->multiplexing_) {
        this->multiplexing_ = true;
        this->start_poller();
      }
    } else {
      // Zero or one channel active — drive pins directly, no multiplexing needed.
      if (this->multiplexing_) {
        this->multiplexing_ = false;
        this->stop_poller();
      }
      this->pina_pin_->set_level(new_pina);
      this->pinb_pin_->set_level(new_pinb);
    }
  }

 protected:
  static constexpr uint32_t SWITCH_INTERVAL_MS = 1;
  output::FloatOutput *pina_pin_;
  output::FloatOutput *pinb_pin_;
  float pina_duty_{0};
  float pinb_duty_{0};
  bool forward_direction_{false};
  bool multiplexing_{false};
};

}  // namespace esphome::hbridge
