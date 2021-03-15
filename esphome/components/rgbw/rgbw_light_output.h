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
  void set_max_brightness(float max_brightness) { max_brightness_ = max_brightness; }
  void set_min_brightness(float min_brightness) { min_brightness_ = min_brightness; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supports_brightness(true);
    traits.set_supports_color_interlock(this->color_interlock_);
    traits.set_supports_rgb(true);
    traits.set_supports_rgb_white_value(true);
    return traits;
  }
  void write_state(light::LightState *state) override {
    float red, green, blue, white;
    state->current_values_as_rgbw(&red, &green, &blue, &white, this->color_interlock_);
    if (min_brightness_ != 0.0f || max_brightness_ != 1.0f) {
      red = red == 0.0f ? 0.0f : min_brightness_ + red * (max_brightness_ - min_brightness_);
      green = green == 0.0f ? 0.0f : min_brightness_ + green * (max_brightness_ - min_brightness_);
      blue = blue == 0.0f ? 0.0f : min_brightness_ + blue * (max_brightness_ - min_brightness_);
      white = white == 0.0f ? 0.0f : min_brightness_ + white * (max_brightness_ - min_brightness_);
    }
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
  float max_brightness_{1.0f};
  float min_brightness_{0.0f};
};

}  // namespace rgbw
}  // namespace esphome
