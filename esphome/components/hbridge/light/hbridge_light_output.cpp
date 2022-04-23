#include "hbridge_light_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hbridge {

static const char *const TAG = "light.hbridge";
// Let H-Bridge base component use our log tag
const char *HBridgeLightOutput::get_log_tag() { return TAG; }

light::LightTraits HBridgeLightOutput::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::COLD_WARM_WHITE});
  traits.set_min_mireds(153);
  traits.set_max_mireds(500);
  return traits;
}

// Component interfacing/overriding from HBridge base class
void HBridgeLightOutput::setup() {
  this->direction_a_update_ = false;
  HBridge::setup();

  // Force "fast" decay mode, lights do not support slow decay
  HBridge::set_hbridge_decay_mode(CurrentDecayMode::FAST);
}

void HBridgeLightOutput::loop() {
  if (light_direction_a_duty_ == 0 && light_direction_b_duty_ == 0) {
    // Light is off, turn-off HBridge
    HBridge::set_output_state_(HBridgeMode::OFF, 0);  // Use protected function to prevent a flood of debug prints
  } else if (light_direction_a_duty_ > 0 && light_direction_b_duty_ == 0) {
    // Only A side is enabled, only drive that side
    HBridge::set_output_state_(
        HBridgeMode::DIRECTION_A,
        this->light_direction_a_duty_);  // Use protected function to prevent a flood of debug prints
  } else if (light_direction_a_duty_ == 0 && light_direction_b_duty_ > 0) {
    // Only B side is enabled, only drive that side
    HBridge::set_output_state_(
        HBridgeMode::DIRECTION_B,
        this->light_direction_b_duty_);  // Use protected function to prevent a flood of debug prints
  } else {
    // A combination of A and B is enabled, drive both by alternating each cycle.
    // This might/will flicker, this could be solved by an advanced PWM implementation with an offset in the dutycycle

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
  }

  HBridge::loop();
}

void HBridgeLightOutput::write_state(light::LightState *state) {
  state->current_values_as_cwww(&this->light_direction_a_duty_, &this->light_direction_b_duty_, false);
  ESP_LOGD(TAG, "New light state A: %f B: %f", this->light_direction_a_duty_, this->light_direction_b_duty_);
}

}  // namespace hbridge
}  // namespace esphome
