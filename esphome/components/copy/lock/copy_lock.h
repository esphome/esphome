#pragma once

#include "esphome/core/component.h"
#include "esphome/components/lock/lock.h"

namespace esphome {
namespace copy {

class CopyLock : public lock::Lock, public Component {
 public:
  void set_source(lock::Lock *source) { source_ = source; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(const lock::LockCall &call) override;

  lock::Lock *source_;
};

}  // namespace copy
}  // namespace esphome
