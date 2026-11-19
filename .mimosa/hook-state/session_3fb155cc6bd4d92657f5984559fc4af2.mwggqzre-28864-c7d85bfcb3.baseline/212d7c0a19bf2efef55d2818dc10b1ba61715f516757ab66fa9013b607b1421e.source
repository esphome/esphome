#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/pixoo/pixoo.h"
#include "esphome/core/helpers.h"

namespace esphome::pixoo {

// Brightness-only light that drives the Pixoo panel's LIGHT command.
class PixooLight : public light::LightOutput, public Parented<Pixoo> {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    return traits;
  }

  void write_state(light::LightState *state) override {
    float brightness;
    state->current_values_as_brightness(&brightness);
    this->parent_->set_panel_brightness(brightness);
  }
};

}  // namespace esphome::pixoo
