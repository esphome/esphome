#pragma once

#ifdef USE_RP2040

#include "esphome/components/output/float_output.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace rp2040_pwm {

class RP2040PWM : public output::FloatOutput, public Component {
 public:
  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void set_frequency(float frequency) { this->frequency_ = frequency; }
  /// Dynamically update frequency
  void update_frequency(float frequency) override {
    this->set_frequency(frequency);
    this->frequency_changed_ = true;
    this->write_state(this->last_output_);
  }

  /// Initialize pin
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup_priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;

  void setup_pwm_();

  InternalGPIOPin *pin_;
  float frequency_{1000.0};
  uint16_t wrap_{65535};
  /// Cache last output level for dynamic frequency updating
  float last_output_{0.0};
  bool frequency_changed_{false};
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...> {
 public:
  SetFrequencyAction(RP2040PWM *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, frequency);

  void play(Ts... x) {
    float freq = this->frequency_.value(x...);
    this->parent_->update_frequency(freq);
  }

  RP2040PWM *parent_;
};

}  // namespace rp2040_pwm
}  // namespace esphome

#endif  // USE_RP2040
