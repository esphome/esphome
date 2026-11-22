#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome::interval {

/// Base of the interval components; holds the first update back for a startup delay.
class IntervalBase : public PollingComponent {
 protected:
  void start_after_delay_(uint32_t startup_delay);
};

/// Calls Fn from update(), for interval.
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
