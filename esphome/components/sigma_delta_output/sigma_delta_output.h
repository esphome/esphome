#pragma once
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace sigma_delta_output {

class SigmaDeltaOutput : public PollingComponent, public output::FloatOutput {
 public:
  Trigger<> *get_turn_on_trigger() {
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

  void set_pin(GPIOPin *pin) { this->pin_ = pin; };
  void write_state(float state) override { this->state_ = state; }
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  GPIOPin *pin_{nullptr};

  std::unique_ptr<Trigger<>> turn_on_trigger_{nullptr};
  std::unique_ptr<Trigger<>> turn_off_trigger_{nullptr};
  std::unique_ptr<Trigger<bool>> state_change_trigger_{nullptr};

  float accum_{0};
  float state_{0.};
  bool value_{false};
};
}  // namespace sigma_delta_output
}  // namespace esphome
