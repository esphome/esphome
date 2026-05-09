#pragma once

#include "esphome/components/lock/lock.h"

namespace esphome::demo {

class DemoLock : public lock::Lock {
 protected:
  void control(const lock::LockCall &call) override {
    auto state = call.get_state();
    if (state.has_value())
      this->publish_state(*state);
  }
};

}  // namespace esphome::demo
