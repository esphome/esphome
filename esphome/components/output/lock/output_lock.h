#pragma once

#include "esphome/core/component.h"
#include "esphome/components/lock/lock.h"
#include "esphome/components/output/binary_output.h"

namespace esphome {
namespace output {

class OutputLock : public lock::Lock, public Component {
 public:
  void set_output(BinaryOutput *output) { output_ = output; }

  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }
  void dump_config() override;

 protected:
  void control(const lock::LockCall &call) override;

  output::BinaryOutput *output_;
};

}  // namespace output
}  // namespace esphome
