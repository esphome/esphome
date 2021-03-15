#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace monochromatic {

class MonochromaticLightOutput : public light::LightOutput {
 public:
  void set_output(output::FloatOutput *output) { output_ = output; }
  void set_max_brightness(float max_brightness) { max_brightness_ = max_brightness; }
  void set_min_brightness(float min_brightness) { min_brightness_ = min_brightness; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supports_brightness(true);
    return traits;
  }
  void write_state(light::LightState *state) override {
    float bright;
    if (min_brightness_ != 0.0f || max_brightness_ != 1.0f) {
      bright = bright == 0.0f ? 0.0f : min_brightness_ + bright * (max_brightness_ - min_brightness_);
    }
    state->current_values_as_brightness(&bright);
    this->output_->set_level(bright);
  }

 protected:
  output::FloatOutput *output_;
  float max_brightness_{1.0f};
  float min_brightness_{0.0f};
};

}  // namespace monochromatic
}  // namespace esphome
