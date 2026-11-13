#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/automation.h"

namespace esphome::interval {

/// Base of the interval components; holds the first update back for a startup delay.
class IntervalBase : public PollingComponent {
 protected:
  void start_after_delay_(uint32_t startup_delay);
};

class IntervalTrigger : public Trigger<>, public IntervalBase {
 public:
  void update() override { this->trigger(); }
};

/// An interval whose first run waits for a startup delay; only used when the delay is not zero.
class DelayedIntervalTrigger final : public IntervalTrigger {
 public:
  explicit DelayedIntervalTrigger(uint32_t startup_delay) : startup_delay_(startup_delay) {}

  void setup() override { this->start_after_delay_(this->startup_delay_); }

 protected:
  const uint32_t startup_delay_;
};

/// Calls Fn from update(), for an interval without a hand-set id.
template<auto Fn> class IntervalCallback : public IntervalBase {
 public:
  void update() override { Fn(); }
};

/// An IntervalCallback whose first run waits STARTUP_DELAY ms; only used when the delay is not zero.
template<auto Fn, uint32_t STARTUP_DELAY> class DelayedIntervalCallback final : public IntervalCallback<Fn> {
 public:
  void setup() override { this->start_after_delay_(STARTUP_DELAY); }
};

}  // namespace esphome::interval
