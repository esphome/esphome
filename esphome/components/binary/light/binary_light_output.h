#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace binary {

class BinaryLightOutput : public light::LightOutput {
 public:
  void set_output(output::BinaryOutput *output) {
    this->output_ = output;

    this->output_->add_on_state_callback([this](bool state) { this->state_callback_.call(state); });
  }
  void add_on_state_callback(std::function<void(bool)> &&callback) { this->state_callback_.add(std::move(callback)); }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::ON_OFF});
    return traits;
  }
  void write_state(light::LightState *state) override {
    bool binary;
    state->current_values_as_binary(&binary);
    if (binary)
      this->output_->turn_on();
    else
      this->output_->turn_off();
  }

 protected:
  output::BinaryOutput *output_;
};

}  // namespace binary
}  // namespace esphome
