#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace binary {

class BinaryLightOutput : public light::LightOutput {
 public:
  void set_output(output::BinaryOutput *output) { output_ = output; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::ON_OFF});
    return traits;
  }
  void write_state(light::LightState *state) override {
    bool binary;
    state->current_values_as_binary(&binary);
    if (binary) {
      this->output_->turn_on();
    } else {
      this->output_->turn_off();
    }
  }

 protected:
  output::BinaryOutput *output_;
};

}  // namespace binary
}  // namespace esphome
