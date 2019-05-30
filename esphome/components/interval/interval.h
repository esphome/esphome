#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace interval {

class IntervalTrigger : public Trigger<>, public PollingComponent {
 public:
  void update() override { this->trigger(); }
  float get_setup_priority() const override { return setup_priority::DATA; }
};

}  // namespace interval
}  // namespace esphome
