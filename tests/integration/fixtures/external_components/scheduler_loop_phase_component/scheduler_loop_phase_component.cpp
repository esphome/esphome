#include "scheduler_loop_phase_component.h"

#include "esphome/core/log.h"

namespace esphome::scheduler_loop_phase_component {

static const char *const TAG = "scheduler_loop_phase_component";

void SchedulerLoopPhaseComponent::loop() {
  if (++this->loop_count_ != 2)
    return;

  const uint32_t start_time = millis();
  this->set_timeout(100, [start_time]() {
    ESP_LOGI(TAG, "SCHEDULER_LOOP_PHASE_RESULT elapsed=%u", static_cast<unsigned int>(millis() - start_time));
  });
}

}  // namespace esphome::scheduler_loop_phase_component
