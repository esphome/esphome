#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace cwww {

class CWWWLightOutput : public light::LightOutput {
 public:
  void set_cold_white(output::FloatOutput *cold_white) { cold_white_ = cold_white; }
  void set_warm_white(output::FloatOutput *warm_white) { warm_white_ = warm_white; }
  void set_cold_white_temperature(float cold_white_temperature) { cold_white_temperature_ = cold_white_temperature; }
  void set_warm_white_temperature(float warm_white_temperature) { warm_white_temperature_ = warm_white_temperature; }
  void set_constant_brightness(bool constant_brightness) { constant_brightness_ = constant_brightness; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supports_brightness(true);
    traits.set_supports_rgb(false);
    traits.set_supports_rgb_white_value(false);
    traits.set_supports_color_temperature(true);
    traits.set_min_mireds(this->cold_white_temperature_);
    traits.set_max_mireds(this->warm_white_temperature_);
    return traits;
  }
  void write_state(light::LightState *state) override {
    float cwhite, wwhite;
    state->current_values_as_cwww(&cwhite, &wwhite, this->constant_brightness_);
    this->cold_white_->set_level(cwhite);
    this->warm_white_->set_level(wwhite);
  }

 protected:
  output::FloatOutput *cold_white_;
  output::FloatOutput *warm_white_;
  float cold_white_temperature_;
  float warm_white_temperature_;
  bool constant_brightness_;
};

}  // namespace cwww
}  // namespace esphome
