#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace slow_pwm {

class SlowPWMOutput : public output::FloatOutput, public Component {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; };
  void set_period(unsigned int period) { period_ = period; };

  /// Initialize pin
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup_priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;
  void loop() override;

  GPIOPin *pin_;
  float state_{0};
  unsigned int period_start_time_{0};
  unsigned int period_{5000};
};

}  // namespace slow_pwm
}  // namespace esphome
