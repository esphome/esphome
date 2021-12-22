#pragma once
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/switch/switch.h"

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

  Trigger<> *get_turn_on_trigger() {
    // Lazy create
    if (!turn_on_trigger_)
      turn_on_trigger_ = make_unique<Trigger<>>();
    return turn_on_trigger_.get();
  }
  Trigger<> *get_turn_off_trigger() {
    if (!turn_off_trigger_)
      turn_off_trigger_ = make_unique<Trigger<>>();
    return turn_off_trigger_.get();
  }

  Trigger<bool> *get_state_change_trigger() {
    if (!state_change_trigger_)
      state_change_trigger_ = make_unique<Trigger<bool>>();
    return state_change_trigger_.get();
  }

 protected:
  /// turn on/off the configured output
  void set_state_(bool state);
  void loop() override;

  GPIOPin *pin_{nullptr};

  void write_state(float state) override { state_ = state; }

  std::unique_ptr<Trigger<>> turn_on_trigger_{nullptr};
  std::unique_ptr<Trigger<>> turn_off_trigger_{nullptr};
  std::unique_ptr<Trigger<bool>> state_change_trigger_{nullptr};
  float state_;
  unsigned int period_start_time_{0};
  unsigned int period_{5000};
};

}  // namespace slow_pwm
}  // namespace esphome
