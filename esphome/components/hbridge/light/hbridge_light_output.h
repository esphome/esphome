#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/log.h"
#include "esphome/components/hbridge/hbridge.h"

namespace esphome {
namespace hbridge {

// Using PollingComponent as the updates are more consistent and reduces flickering
class HBridgeLightOutput : public PollingComponent, public light::LightOutput, public hbridge::HBridge {
 public:
  HBridgeLightOutput() : PollingComponent(1) {}

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::COLD_WARM_WHITE});
    traits.set_min_mireds(153);
    traits.set_max_mireds(500);
    return traits;
  }

  void setup() override { 
    this->direction_a_update_ = false; 
    HBridge::setup();
  }

  void loop() override { 
    HBridge::loop();
  }

  void update() override {
    // This method runs around 60 times per second
    // We cannot do the PWM ourselves so we are reliant on the hardware PWM

    //Alternate updating/flashing each light direction
    if (!this->direction_a_update_) {  // First LED Direction
      HBridge::set_output_state(HBRIDGE_MODE_DIRECTION_A, this->light_direction_a_duty_); //Use protected function to prevent a flood of debug prints
      this->direction_a_update_ = true;
    } else {  // Second LED Direction
      HBridge::set_output_state(HBRIDGE_MODE_DIRECTION_B, this->light_direction_b_duty_); //Use protected function to prevent a flood of debug prints
      this->direction_a_update_ = false;
    }
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void write_state(light::LightState *state) override {
    state->current_values_as_cwww(&this->light_direction_a_duty_, &this->light_direction_b_duty_, false);
  }

 protected:
  float light_direction_a_duty_ = 0;
  float light_direction_b_duty_ = 0;
  bool direction_a_update_ = false;
};

}  // namespace hbridge
}  // namespace esphome
