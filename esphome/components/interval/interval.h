#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace interval {

class IntervalTrigger : public Trigger<>, public PollingComponent {
 public:
  IntervalTrigger(uint32_t update_interval) : PollingComponent(update_interval) {}
  void update() override { this->trigger(); }
  float get_setup_priority() const override { return setup_priority::DATA; }
};

}  // namespace interval
}  // namespace esphome
