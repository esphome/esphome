#pragma once

#include "esphome/components/lock/lock.h"

namespace esphome {
namespace demo {

class DemoLock : public lock::Lock {
 protected:
  void control(const lock::LockCall &call) override {
    auto state = *call.get_state();
    this->publish_state(state);
  }
};

}  // namespace demo
}  // namespace esphome
