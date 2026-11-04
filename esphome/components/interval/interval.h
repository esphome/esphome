#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/automation.h"

namespace esphome::interval {

class IntervalTrigger : public Trigger<>, public PollingComponent {
 public:
  void update() override { this->trigger(); }
};

/// An interval whose first run waits for a startup delay; only used when the delay is not zero.
class DelayedIntervalTrigger final : public IntervalTrigger {
 public:
  explicit DelayedIntervalTrigger(uint32_t startup_delay) : startup_delay_(startup_delay) {}

  void setup() override {
    this->stop_poller();
    this->set_timeout(this->startup_delay_, [this] { this->start_poller(); });
  }

 protected:
  const uint32_t startup_delay_;
};

}  // namespace esphome::interval
