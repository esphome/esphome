#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace loop_test_component {

class LoopTestISRComponent : public Component {
 public:
  void set_name(const std::string &name) { this->name_ = name; }

  void setup() override;
  void loop() override;

  // Simulates an ISR calling enable_loop_soon_any_context
  void simulate_isr_enable();

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  std::string name_;
  int loop_count_{0};
  uint32_t last_disable_time_{0};
  uint32_t last_check_time_{0};
  bool isr_enable_pending_{false};
  int isr_call_count_{0};
};

}  // namespace loop_test_component
}  // namespace esphome
