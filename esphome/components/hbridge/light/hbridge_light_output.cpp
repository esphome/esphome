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

void HBridgeLightOutput::write_state(light::LightState *state) {
  state->current_values_as_cwww(&this->light_direction_a_duty_, &this->light_direction_b_duty_, false);
  ESP_LOGD(TAG, "HBridgeLight", "New light state A: %f B: %f", this->light_direction_a_duty_,
           this->light_direction_b_duty_);
}

}  // namespace hbridge
}  // namespace esphome
