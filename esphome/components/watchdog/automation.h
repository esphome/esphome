#pragma once
#include "watchdog.h"

#include "esphome/core/automation.h"

namespace esphome::watchdog {

template<typename... Ts> class WatchdogManagerComponentTimeoutAction : public Action<Ts...> {
 public:
  WatchdogManagerComponentTimeoutAction(WatchdogManagerComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint32_t, timeout_ms)

  void play(const Ts &...x) override {
    if (this->timeout_ms_.has_value()) {
      uint32_t timeout_ms = this->timeout_ms_.value(x...);
      if (timeout_ms > 0) {
        this->parent_->set_timeout(timeout_ms);
      }
    }
  }

 protected:
  WatchdogManagerComponent *parent_;
};
}  // namespace esphome::watchdog
