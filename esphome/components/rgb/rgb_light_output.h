#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace rgb {

class RGBLightOutput : public light::LightOutput {
 public:
  void set_red(output::FloatOutput *red) { red_ = red; }
  void set_green(output::FloatOutput *green) { green_ = green; }
  void set_blue(output::FloatOutput *blue) { blue_ = blue; }
  void set_max_brightness(float max_brightness) { max_brightness_ = max_brightness; }
  void set_min_brightness(float min_brightness) { min_brightness_ = min_brightness; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supports_brightness(true);
    traits.set_supports_rgb(true);
    return traits;
  }
  void write_state(light::LightState *state) override {
    float red, green, blue;
    state->current_values_as_rgb(&red, &green, &blue, false);
    if (min_brightness_ != 0.0f || max_brightness_ != 1.0f) {
      red = red == 0.0f ? 0.0f : min_brightness_ + red * (max_brightness_ - min_brightness_);
      green = green == 0.0f ? 0.0f : min_brightness_ + green * (max_brightness_ - min_brightness_);
      blue = blue == 0.0f ? 0.0f : min_brightness_ + blue * (max_brightness_ - min_brightness_);
    }
    this->red_->set_level(red);
    this->green_->set_level(green);
    this->blue_->set_level(blue);
  }

 protected:
  output::FloatOutput *red_;
  output::FloatOutput *green_;
  output::FloatOutput *blue_;
  float max_brightness_{1.0f};
  float min_brightness_{0.0f};
};

}  // namespace rgb
}  // namespace esphome
