#pragma once
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace slow_pwm {

class SlowPWMOutput : public output::FloatOutput, public Component {
 public:
  void set_pin(GPIOPin *pin) { pin_ = pin; };
  void set_period(unsigned int period) { period_ = period; };
  void set_restart_cycle_on_state_change(bool restart_cycle_on_state_change) {
    restart_cycle_on_state_change_ = restart_cycle_on_state_change;
  }
  void restart_cycle() { this->period_start_time_ = millis(); }

  /// Initialize pin
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup_priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  Trigger<> *get_turn_on_trigger() {
    // Lazy create
    if (!this->turn_on_trigger_)
      this->turn_on_trigger_ = make_unique<Trigger<>>();
    return this->turn_on_trigger_.get();
  }
  Trigger<> *get_turn_off_trigger() {
    if (!this->turn_off_trigger_)
      this->turn_off_trigger_ = make_unique<Trigger<>>();
    return this->turn_off_trigger_.get();
  }

  Trigger<bool> *get_state_change_trigger() {
    if (!this->state_change_trigger_)
      this->state_change_trigger_ = make_unique<Trigger<bool>>();
    return this->state_change_trigger_.get();
  }

 protected:
  void loop() override;
  void write_state(float state) override;
  /// turn on/off the configured output
  void set_output_state_(bool state);

  GPIOPin *pin_{nullptr};
  std::unique_ptr<Trigger<>> turn_on_trigger_{nullptr};
  std::unique_ptr<Trigger<>> turn_off_trigger_{nullptr};
  std::unique_ptr<Trigger<bool>> state_change_trigger_{nullptr};
  float state_{0};
  bool current_state_{false};
  unsigned int period_start_time_{0};
  unsigned int period_;
  bool restart_cycle_on_state_change_;
};

}  // namespace slow_pwm
}  // namespace esphome
