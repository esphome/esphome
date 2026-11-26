#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome::cwww {

class CWWWLightOutput final : public light::LightOutput {
 public:
  void set_cold_white(output::FloatOutput *cold_white) { cold_white_ = cold_white; }
  void set_warm_white(output::FloatOutput *warm_white) { warm_white_ = warm_white; }
  void set_cold_white_temperature(float cold_white_temperature) { cold_white_temperature_ = cold_white_temperature; }
  void set_warm_white_temperature(float warm_white_temperature) { warm_white_temperature_ = warm_white_temperature; }
  void set_constant_brightness(light::ConstantBrightness constant_brightness) {
    this->constant_brightness_ = constant_brightness;
  }
  // Lambdas from before color_temperature_priority pass a bool
  void set_constant_brightness(bool constant_brightness) {
    this->set_constant_brightness(constant_brightness ? light::ConstantBrightness::CONSTANT_BRIGHTNESS_ENABLED
                                                      : light::ConstantBrightness::CONSTANT_BRIGHTNESS_DISABLED);
  }
#ifdef USE_LIGHT_WHITE_RELATIVE_BRIGHTNESS
  /// Light of each white channel at full output, the stronger 1.0; see LightState::current_values_as_cwww()
  void set_white_relative_brightness(float cold_white, float warm_white) {
    this->cold_white_relative_brightness_ = cold_white;
    this->warm_white_relative_brightness_ = warm_white;
  }
#endif
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::COLD_WARM_WHITE});
    traits.set_min_mireds(this->cold_white_temperature_);
    traits.set_max_mireds(this->warm_white_temperature_);
    return traits;
  }
  void write_state(light::LightState *state) override {
    float cwhite, wwhite;
#ifdef USE_LIGHT_WHITE_RELATIVE_BRIGHTNESS
    state->current_values_as_cwww(&cwhite, &wwhite, this->constant_brightness_, this->cold_white_relative_brightness_,
                                  this->warm_white_relative_brightness_);
#else
    state->current_values_as_cwww(&cwhite, &wwhite, this->constant_brightness_);
#endif
    this->cold_white_->set_level(cwhite);
    this->warm_white_->set_level(wwhite);
  }

 protected:
  output::FloatOutput *cold_white_;
  output::FloatOutput *warm_white_;
  float cold_white_temperature_{0};
  float warm_white_temperature_{0};
#ifdef USE_LIGHT_WHITE_RELATIVE_BRIGHTNESS
  float cold_white_relative_brightness_{1.0f};
  float warm_white_relative_brightness_{1.0f};
#endif
  light::ConstantBrightness constant_brightness_{light::ConstantBrightness::CONSTANT_BRIGHTNESS_DISABLED};
};

}  // namespace esphome::cwww
