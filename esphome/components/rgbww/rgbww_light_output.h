#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace rgbww {

class RGBWWLightOutput : public light::LightOutput {
 public:
  RGBWWLightOutput(output::FloatOutput *red, output::FloatOutput *green, output::FloatOutput *blue,
                   output::FloatOutput *cold_white, output::FloatOutput *warm_white, float cold_white_temperature,
                   float warm_white_temperature)
      : red_(red),
        green_(green),
        blue_(blue),
        cold_white_(cold_white),
        warm_white_(warm_white),
        cold_white_temperature_(cold_white_temperature),
        warm_white_temperature_(warm_white_temperature) {}
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supports_brightness(true);
    traits.set_supports_rgb(true);
    traits.set_supports_rgb_white_value(true);
    traits.set_supports_color_temperature(true);
    return traits;
  }
  void write_state(light::LightState *state) override {
    float red, green, blue, cwhite, wwhite;
    state->current_values_as_rgbww(&red, &green, &blue, &cwhite, &wwhite);
    this->red_->set_level(red);
    this->green_->set_level(green);
    this->blue_->set_level(blue);
    this->cold_white_->set_level(cwhite);
    this->warm_white_->set_level(wwhite);
  }

 protected:
  output::FloatOutput *red_;
  output::FloatOutput *green_;
  output::FloatOutput *blue_;
  output::FloatOutput *cold_white_;
  output::FloatOutput *warm_white_;
  float cold_white_temperature_;
  float warm_white_temperature_;
};

}  // namespace rgbww
}  // namespace esphome
