#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace rgbw {

class RGBWLightOutput : public light::LightOutput {
 public:
  void set_red(output::FloatOutput *red) { red_ = red; }
  void set_green(output::FloatOutput *green) { green_ = green; }
  void set_blue(output::FloatOutput *blue) { blue_ = blue; }
  void set_white(output::FloatOutput *white) { white_ = white; }
  void set_color_interlock(bool color_interlock) { color_interlock_ = color_interlock; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    if (this->color_interlock_)
      traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::WHITE});
    else
      traits.set_supported_color_modes({light::ColorMode::RGB_WHITE});
    return traits;
  }
  void write_state(light::LightState *state) override {
    float red, green, blue, white;
    state->current_values_as_rgbw(&red, &green, &blue, &white, this->color_interlock_);
    this->red_->set_level(red);
    this->green_->set_level(green);
    this->blue_->set_level(blue);
    this->white_->set_level(white);
  }

 protected:
  output::FloatOutput *red_;
  output::FloatOutput *green_;
  output::FloatOutput *blue_;
  output::FloatOutput *white_;
  bool color_interlock_{false};
};

}  // namespace rgbw
}  // namespace esphome
