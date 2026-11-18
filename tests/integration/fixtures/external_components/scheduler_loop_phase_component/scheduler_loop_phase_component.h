#pragma once

#include "esphome/core/component.h"

namespace esphome::scheduler_loop_phase_component {

class SchedulerLoopPhaseComponent : public Component {
 public:
  void loop() override;

 protected:
  uint8_t loop_count_{0};
};

}  // namespace esphome::scheduler_loop_phase_component
