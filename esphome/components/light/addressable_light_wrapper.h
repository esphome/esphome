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
    float gamma = this->light_state_->get_gamma_correct();
    float r = gamma_uncorrect(this->wrapper_state_[0] / 255.0f, gamma);
    float g = gamma_uncorrect(this->wrapper_state_[1] / 255.0f, gamma);
    float b = gamma_uncorrect(this->wrapper_state_[2] / 255.0f, gamma);
    float w = gamma_uncorrect(this->wrapper_state_[3] / 255.0f, gamma);
    float brightness = fmaxf(r, fmaxf(g, b));

    auto call = this->light_state_->make_call();
    call.set_state(true);
    call.set_brightness_if_supported(1.0f);
    call.set_color_brightness_if_supported(brightness);
    call.set_red_if_supported(r);
    call.set_green_if_supported(g);
    call.set_blue_if_supported(b);
    call.set_white_if_supported(w);
    call.set_transition_length_if_supported(0);
    call.set_publish(false);
    call.set_save(false);
    call.perform();

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
