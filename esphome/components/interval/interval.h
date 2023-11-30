#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace interval {

class IntervalTrigger : public Trigger<>, public PollingComponent {
 public:
  void update() override {
    if (this->started_)
      this->trigger();
  }

  void setup() override {
    if (this->startup_delay_ == 0) {
      this->started_ = true;
    } else {
      this->set_timeout(this->startup_delay_, [this] { this->started_ = true; });
    }
  }

  void set_startup_delay(const uint32_t startup_delay) { this->startup_delay_ = startup_delay; }

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  uint32_t startup_delay_{0};
  bool started_{false};
};

}  // namespace interval
}  // namespace esphome
