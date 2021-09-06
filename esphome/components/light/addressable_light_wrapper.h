#pragma once

#include "esphome/core/component.h"
#include "addressable_light.h"

namespace esphome {
namespace light {

class AddressableLightWrapper : public light::AddressableLight {
 public:
  explicit AddressableLightWrapper(light::LightState *light_state) : light_state_(light_state) {
    this->wrapper_state_ = new uint8_t[5];
  }

  int32_t size() const override { return 1; }

  void clear_effect_data() override { this->wrapper_state_[4] = 0; }

  light::LightTraits get_traits() override { return this->light_state_->get_traits(); }

  void write_state(light::LightState *state) override {
    light::LightColorValues *ls = &state->current_values;
    ls->set_brightness(1.0f);
    ls->set_color_brightness(1.0f);
    ls->set_state(1.0f);
    ls->set_red(this->wrapper_state_[0] / 255.0f);
    ls->set_green(this->wrapper_state_[1] / 255.0f);
    ls->set_blue(this->wrapper_state_[2] / 255.0f);
    ls->set_white(this->wrapper_state_[3] / 255.0f);

    this->light_state_->get_output()->write_state(state);
    this->mark_shown_();
  }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {
    return {&this->wrapper_state_[0], &this->wrapper_state_[1], &this->wrapper_state_[2],
            &this->wrapper_state_[3], &this->wrapper_state_[4], &this->correction_};
  }

  light::LightState *light_state_;
  uint8_t *wrapper_state_;
};

}  // namespace light
}  // namespace esphome
