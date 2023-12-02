#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace rgbww {

class RGBWWLightOutput : public light::LightOutput {
 public:
  void set_red(output::FloatOutput *red) { red_ = red; }
  void set_green(output::FloatOutput *green) { green_ = green; }
  void set_blue(output::FloatOutput *blue) { blue_ = blue; }
  void set_cold_white(output::FloatOutput *cold_white) { cold_white_ = cold_white; }
  void set_warm_white(output::FloatOutput *warm_white) { warm_white_ = warm_white; }
  void set_cold_white_temperature(float cold_white_temperature) { cold_white_temperature_ = cold_white_temperature; }
  void set_warm_white_temperature(float warm_white_temperature) { warm_white_temperature_ = warm_white_temperature; }
  void set_constant_brightness(bool constant_brightness) { constant_brightness_ = constant_brightness; }
  void set_color_interlock(bool color_interlock) { color_interlock_ = color_interlock; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    if (this->color_interlock_)
      traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::COLD_WARM_WHITE});
    else
      traits.set_supported_color_modes({light::ColorMode::RGB_COLD_WARM_WHITE});
    traits.set_min_mireds(this->cold_white_temperature_);
    traits.set_max_mireds(this->warm_white_temperature_);
    return traits;
  }
  void write_state(light::LightState *state) override {
    float red, green, blue, cwhite, wwhite;
    state->current_values_as_rgbww(&red, &green, &blue, &cwhite, &wwhite, this->constant_brightness_);
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
  float cold_white_temperature_{0};
  float warm_white_temperature_{0};
  bool constant_brightness_;
  bool color_interlock_{false};
};

}  // namespace rgbww
}  // namespace esphome
