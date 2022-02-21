#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/log.h"
#include "esphome/components/hbridge/hbridge.h"

namespace esphome {
namespace hbridge {

// Using PollingComponent as the updates are more consistent and reduces flickering
class HBridgeLightOutput : public HBridge, public light::LightOutput {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::COLD_WARM_WHITE});
    traits.set_min_mireds(153);
    traits.set_max_mireds(500);
    return traits;
  }

  // Component interfacing/overriding from HBridge base class
  void setup() override {
    this->direction_a_update_ = false;
    HBridge::setup();

    // Force "fast" decay mode, lights do not support slow decay
    HBridge::set_hbridge_decay_mode(CurrentDecayMode::FAST);
  }

  void loop() override {
    // This method runs every loop. This is a best-effort situation.

    // Alternate updating/flashing each light direction
    if (!this->direction_a_update_) {  // First LED Direction
      HBridge::set_output_state_(
          HBridgeMode::DIRECTION_A,
          this->light_direction_a_duty_);  // Use protected function to prevent a flood of debug prints
      this->direction_a_update_ = true;
    } else {  // Second LED Direction
      HBridge::set_output_state_(
          HBridgeMode::DIRECTION_B,
          this->light_direction_b_duty_);  // Use protected function to prevent a flood of debug prints
      this->direction_a_update_ = false;
    }

    HBridge::loop();
  }

  void write_state(light::LightState *state) override {
    state->current_values_as_cwww(&this->light_direction_a_duty_, &this->light_direction_b_duty_, false);
    ESP_LOGD("HBridgeLight", "New light state A: %f B: %f", this->light_direction_a_duty_,
             this->light_direction_b_duty_);
  }

 protected:
  float light_direction_a_duty_ = 0;
  float light_direction_b_duty_ = 0;
  bool direction_a_update_ = false;
};

}  // namespace hbridge
}  // namespace esphome
