#pragma once

#ifdef USE_ESP8266

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"

namespace esphome::esp8266_pwm {

class ESP8266PWM final : public output::FloatOutput, public Component {
 public:
  // User provided, not "= default": `new(p) ESP8266PWM()` would zero-fill .bss that is already zero.
  ESP8266PWM() {}

  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void set_frequency(float frequency) { this->frequency_ = frequency; }
  /// Dynamically update frequency
  void update_frequency(float frequency) override {
    this->set_frequency(frequency);
    this->write_state(this->last_output_);
  }

  /// Initialize pin
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup_priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;

  InternalGPIOPin *pin_{nullptr};
  float frequency_{1000.0};  // Keep in sync with DEFAULT_FREQUENCY in output.py
  /// Cache last output level for dynamic frequency updating
  float last_output_{0.0};
};

}  // namespace esphome::esp8266_pwm

#endif
