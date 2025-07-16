#pragma once

#include "esphome/core/component.h"
#include <atomic>

namespace esphome {
namespace scheduler_heap_stress_component {

class SchedulerHeapStressComponent : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void run_multi_thread_test();

 private:
  std::atomic<int> total_callbacks_{0};
  std::atomic<int> executed_callbacks_{0};
};

}  // namespace scheduler_heap_stress_component
}  // namespace esphome
