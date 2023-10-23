#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/output/binary_output.h"

namespace esphome {
namespace status_led {

class StatusLEDLightOutput : public light::LightOutput, public Component {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }
  void set_output(output::BinaryOutput *output) { output_ = output; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::ON_OFF});
    return traits;
  }

  void loop() override;

  void setup_state(light::LightState *state) override;

  void write_state(light::LightState *state) override;

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  float get_loop_priority() const override { return 50.0f; }

 protected:
  GPIOPin *pin_{nullptr};
  output::BinaryOutput *output_{nullptr};
  light::LightState *lightstate_{};
  uint32_t last_app_state_{0xFFFF};
  void output_state_(bool state);
};

}  // namespace status_led
}  // namespace esphome
