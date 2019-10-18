#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/core/automation.h"
#include "esphome/components/output/float_output.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace ledc {

extern uint8_t next_ledc_channel;

class LEDCOutput : public output::FloatOutput, public Component {
 public:
  explicit LEDCOutput(GPIOPin *pin) : pin_(pin) { this->channel_ = next_ledc_channel++; }

  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_frequency(float frequency) { this->frequency_ = frequency; }
  /// Dynamically change frequency at runtime
  void apply_frequency(float frequency);

  /// Setup LEDC.
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Override FloatOutput's write_state.
  void write_state(float state) override;

 protected:
  GPIOPin *pin_;
  uint8_t channel_{};
  uint8_t bit_depth_{};
  float frequency_{};
  float duty_{0.0f};
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...> {
 public:
  SetFrequencyAction(LEDCOutput *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, frequency);

  void play(Ts... x) {
    float freq = this->frequency_.value(x...);
    this->parent_->apply_frequency(freq);
  }

 protected:
  LEDCOutput *parent_;
};

}  // namespace ledc
}  // namespace esphome

#endif
