#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/automation.h"

namespace esphome::interval {

/// Base of the interval components; holds the first update back for a startup delay.
class IntervalBase : public PollingComponent {
 protected:
  void start_after_delay_(uint32_t startup_delay) {
    this->stop_poller();
    this->set_timeout(startup_delay, [this] { this->start_poller(); });
  }
};

class IntervalTrigger final : public Trigger<>, public IntervalBase {
 public:
  void update() override { this->trigger(); }

  void setup() override {
    if (this->startup_delay_ != 0)
      this->start_after_delay_(this->startup_delay_);
  }

  void set_startup_delay(const uint32_t startup_delay) { this->startup_delay_ = startup_delay; }

 protected:
  uint32_t startup_delay_{0};
};

}  // namespace esphome::interval
