#pragma once

#include "esphome/core/component.h"
#include <atomic>

namespace esphome {
namespace scheduler_simultaneous_callbacks_component {

class SchedulerSimultaneousCallbacksComponent : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void run_simultaneous_callbacks_test();

 private:
  std::atomic<int> total_scheduled_{0};
  std::atomic<int> total_executed_{0};
  std::atomic<int> callbacks_at_once_{0};
  std::atomic<int> max_concurrent_{0};
};

}  // namespace scheduler_simultaneous_callbacks_component
}  // namespace esphome
