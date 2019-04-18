#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace esp8266_pwm {

class ESP8266PWM : public output::FloatOutput, public Component {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; }
  void set_frequency(float frequency) { this->frequency_ = frequency; }

  /// Initialize pin
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup_priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;

  GPIOPin *pin_;
  float frequency_{1000.0};
};

}  // namespace esp8266_pwm
}  // namespace esphome
