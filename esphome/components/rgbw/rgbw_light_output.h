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
  void set_cold_white_temperature(float cold_white_temperature) { cold_white_temperature_ = cold_white_temperature; }
  void set_warm_white_temperature(float warm_white_temperature) { warm_white_temperature_ = warm_white_temperature; }
  void set_blue_white_percentage(float blue_white_percentage) { blue_white_percentage_ = blue_white_percentage; }
  void set_red_white_percentage(float red_white_percentage) { red_white_percentage_ = red_white_percentage; }
  void set_additive_brightness(bool additive_brightness) { additive_brightness_ = additive_brightness; }
  void set_color_interlock(bool color_interlock) { color_interlock_ = color_interlock; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    if (this->color_interlock_)
      if (this->pseudo_color_temperature_) {
        traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::COLD_WARM_WHITE});
        traits.set_min_mireds(this->cold_white_temperature_);
        traits.set_max_mireds(this->warm_white_temperature_);
      } else {
        traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::WHITE});
      }
    else
      traits.set_supported_color_modes({light::ColorMode::RGB_WHITE});
    return traits;
  }
  void write_state(light::LightState *state) override {
    float red, green, blue, white;
    (!this->pseudo_color_temperature_)
        ? state->current_values_as_rgbw(&red, &green, &blue, &white, this->color_interlock_)
        : state->current_values_as_emulated_rgbww(&red, &green, &blue, &white, this->blue_white_percentage_,
                                                  this->red_white_percentage_, this->additive_brightness_);
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
  bool pseudo_color_temperature_{true};
  float cold_white_temperature_{0};
  float warm_white_temperature_{0};
  float blue_white_percentage_{0};
  float red_white_percentage_{0};
  bool additive_brightness_{false};
};

}  // namespace rgbw
}  // namespace esphome
