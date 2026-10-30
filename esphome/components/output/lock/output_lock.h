#pragma once

#include "esphome/components/lock/lock.h"
#include "esphome/components/output/binary_output.h"

namespace esphome::output {

class OutputLock final : public lock::Lock {
 public:
  void set_output(BinaryOutput *output) { output_ = output; }

 protected:
  void control(const lock::LockCall &call) override;

  output::BinaryOutput *output_;
};

}  // namespace esphome::output
