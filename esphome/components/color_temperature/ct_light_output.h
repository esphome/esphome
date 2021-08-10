#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"

namespace esphome {
namespace color_temperature {

class CTLightOutput : public light::LightOutput {
 public:
  void set_color_temperature(output::FloatOutput *color_temperature) { color_temperature_ = color_temperature; }
  void set_brightness(output::FloatOutput *brightness) { brightness_ = brightness; }
  void set_cold_white_temperature(float cold_white_temperature) { cold_white_temperature_ = cold_white_temperature; }
  void set_warm_white_temperature(float warm_white_temperature) { warm_white_temperature_ = warm_white_temperature; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
    traits.set_min_mireds(this->cold_white_temperature_);
    traits.set_max_mireds(this->warm_white_temperature_);
    return traits;
  }
  void write_state(light::LightState *state) override {
    float color_temperature, brightness;
    state->current_values_as_ct(&color_temperature, &brightness);
    this->color_temperature_->set_level(color_temperature);
    this->brightness_->set_level(brightness);
  }

 protected:
  output::FloatOutput *color_temperature_;
  output::FloatOutput *brightness_;
  float cold_white_temperature_;
  float warm_white_temperature_;
};

}  // namespace color_temperature
}  // namespace esphome
