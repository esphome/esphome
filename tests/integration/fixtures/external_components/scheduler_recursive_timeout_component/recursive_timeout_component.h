#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace scheduler_recursive_timeout_component {

class SchedulerRecursiveTimeoutComponent : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void run_recursive_timeout_test();

 private:
  int nested_level_{0};
};

}  // namespace scheduler_recursive_timeout_component
}  // namespace esphome
