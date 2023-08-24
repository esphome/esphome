#pragma once

#ifdef USE_ESP8266

#include "esphome/core/automation.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace esp8266_hw_pwm {

class ESP8266HWPWM : public output::FloatOutput, public Component {
 public:
  void set_frequency(float frequency) { this->frequency_ = frequency; }
  /// Dynamically update frequency
  void update_frequency(float frequency) override {
    this->set_frequency(frequency);
    this->frequency_changed_ = true;
    this->write_state(this->last_output_);
  }

  void setup() override;
  void dump_config() override;
  /// HARDWARE setup_priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

protected:
  void write_state(float state) override;

  float frequency_{1000.0};
  /// Cache last output level for dynamic frequency updating
  float last_output_{0.0};
  bool frequency_changed_{false};

private:
  void _pwm_debug();
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...> {
 public:
  SetFrequencyAction(ESP8266HWPWM *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, frequency);

  void play(Ts... x) {
    float freq = this->frequency_.value(x...);
    this->parent_->update_frequency(freq);
  }

  ESP8266HWPWM *parent_;
};

}  // namespace esp8266_hw_pwm
}  // namespace esphome

#endif  // USE_ESP8266
