#pragma once

#include "esphome/core/component.h"

namespace esphome::interval {

/// Calls Fn from update(), for an interval without a hand-set id.
template<void (*Fn)()> class IntervalCallback : public PollingComponent {
 public:
  void update() override { Fn(); }
};

/// An IntervalCallback whose first run waits STARTUP_DELAY ms; only used when the delay is not zero.
template<void (*Fn)(), uint32_t STARTUP_DELAY> class DelayedIntervalCallback final : public IntervalCallback<Fn> {
 public:
  void setup() override {
    this->stop_poller();
    this->set_timeout(STARTUP_DELAY, [this] { this->start_poller(); });
  }
};

}  // namespace esphome::interval
