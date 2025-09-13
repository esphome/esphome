#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace interval {

class IntervalTrigger : public Trigger<>, public PollingComponent {
 public:
  void update() override { this->trigger(); }

  void setup() override {
    if (this->startup_delay_ != 0) {
      this->stop_poller();
      this->set_timeout(this->startup_delay_, [this] { this->start_poller(); });
    }
  }

  void set_startup_delay(const uint32_t startup_delay) { this->startup_delay_ = startup_delay; }

 protected:
  uint32_t startup_delay_{0};
};

}  // namespace interval
}  // namespace esphome
