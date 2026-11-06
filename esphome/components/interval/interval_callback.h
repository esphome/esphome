#pragma once

#include "interval.h"

namespace esphome::interval {

/// Calls Fn from update(), for an interval without a hand-set id.
template<void (*Fn)()> class IntervalCallback : public IntervalBase {
 public:
  void update() override { Fn(); }
};

/// An IntervalCallback whose first run waits STARTUP_DELAY ms; only used when the delay is not zero.
template<void (*Fn)(), uint32_t STARTUP_DELAY> class DelayedIntervalCallback final : public IntervalCallback<Fn> {
 public:
  void setup() override { this->start_after_delay_(STARTUP_DELAY); }
};

}  // namespace esphome::interval
