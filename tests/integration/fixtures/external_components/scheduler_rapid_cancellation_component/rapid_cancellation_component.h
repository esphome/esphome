#pragma once

#include "esphome/core/component.h"
#include <atomic>

namespace esphome {
namespace scheduler_rapid_cancellation_component {

class SchedulerRapidCancellationComponent : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void run_rapid_cancellation_test();

 private:
  std::atomic<int> total_scheduled_{0};
  std::atomic<int> total_executed_{0};
};

}  // namespace scheduler_rapid_cancellation_component
}  // namespace esphome
