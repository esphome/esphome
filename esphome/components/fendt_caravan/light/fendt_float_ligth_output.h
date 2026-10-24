
#pragma once
#ifdef USE_ESP32
#include "fendt_light_output.h"

namespace esphome::fendt_caravan {

class FendtFloatLightOutput : public FendtLightOutput {
 public:
  FendtFloatLightOutput() : FendtLightOutput(light::ColorMode::BRIGHTNESS) {};

  void setup_state(light::LightState *state) override {
    this->light_state_ = state;

    this->variable_ = new Variable<int>(this->variable_name_, DeviceDecoders::decode_int, Commands::update_toggle<int>,
                                        Commands::update_int);
    this->variable_->set_on_decode_callback([state](const int &value) {
      state->remote_values.set_state(value != 0);
      state->remote_values.set_brightness((float) (value) / 15.0f);
      state->publish_state();
    });
    this->parent_->add_variable(this->variable_);
  }

  void write_state(light::LightState *state) override {
    bool cur_state = this->variable_->get_value() != 0;
    if (cur_state != state->current_values.is_on()) {
      this->variable_->set_value(cur_state ^ 1);
      auto command = this->variable_->get_command();
      this->parent_->get_parent()->send_command(command);
    } else {
      int value = (int) (state->remote_values.get_brightness() * 15.0f);
      this->variable_->set_value(value);
      auto command = this->variable_->get_alt_command();
      this->parent_->get_parent()->send_command(command);
    }
  }
};
}  // namespace esphome::fendt_caravan
#endif
