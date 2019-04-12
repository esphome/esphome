#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_state.h"

namespace esphome {
namespace rgb {

class RGBLightOutput : public light::LightOutput {
 public:
  RGBLightOutput(output::FloatOutput *red, output::FloatOutput *green, output::FloatOutput *blue)
    : red_(red), green_(green), blue_(blue) {

  }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supports_brightness(true);
    traits.set_supports_rgb(true);
    return traits;
  }
  void write_state(light::LightState *state) override {
    float red, green, blue;
    state->current_values_as_rgb(&red, &green, &blue);
    this->red_->set_level(red);
    this->green_->set_level(green);
    this->blue_->set_level(blue);
  }
 protected:
  output::FloatOutput *red_;
  output::FloatOutput *green_;
  output::FloatOutput *blue_;
};

}  // namespace rgb
}  // namespace esphome
