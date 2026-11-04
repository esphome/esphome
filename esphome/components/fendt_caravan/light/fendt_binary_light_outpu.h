#pragma once
#ifdef USE_ESP32
#include "fendt_light_output.h"
namespace esphome::fendt_caravan {

class FendtBinaryLightOutput : public FendtLightOutput {
 public:
  void write_state(light::LightState *state) override {
    if (this->is_on_ != state->remote_values.is_on()) {
      this->parent_->get_parent()->send_command(Commands::update_toggle(this->variable_name_));
      this->is_on_ = state->remote_values.is_on();
    }
  }
};
}  // namespace esphome::fendt_caravan
#endif
