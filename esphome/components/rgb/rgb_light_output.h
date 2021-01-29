#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace rgb {

static const char *TAG = "RGBlight";

class RGBLightOutput : public light::LightOutput {
 public:
  void set_red(output::FloatOutput *red) { red_ = red; }
  void set_green(output::FloatOutput *green) { green_ = green; }
  void set_blue(output::FloatOutput *blue) { blue_ = blue; }
  void set_cold_white_temperature(float cold_white_temperature) { cold_white_temperature_ = cold_white_temperature; }
  void set_warm_white_temperature(float warm_white_temperature) { warm_white_temperature_ = warm_white_temperature; }
  void set_rgb_temperature_emulation(bool rgb_temperature_emulation) {
    rgb_temperature_emulation_ = rgb_temperature_emulation;
    if (rgb_temperature_emulation_) {
      this->color_interlock_ = true;
    }
  }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supports_brightness(true);
    traits.set_supports_rgb(true);
    traits.set_min_mireds(this->cold_white_temperature_);
    traits.set_max_mireds(this->warm_white_temperature_);
    if (this->rgb_temperature_emulation_) {
      traits.set_supports_color_interlock(true);
      traits.set_supports_color_temperature(true);
    }
    return traits;
  }
  void write_state(light::LightState *state) override {
    float red, green, blue;
    state->current_values_as_rgb(&red, &green, &blue, this->color_interlock_, this->rgb_temperature_emulation_);
    this->red_->set_level(red);
    this->green_->set_level(green);
    this->blue_->set_level(blue);
  }

 protected:
  output::FloatOutput *red_;
  output::FloatOutput *green_;
  output::FloatOutput *blue_;
  float cold_white_temperature_;
  float warm_white_temperature_;
  bool color_interlock_ = false;
  bool rgb_temperature_emulation_;
};

}  // namespace rgb
}  // namespace esphome
