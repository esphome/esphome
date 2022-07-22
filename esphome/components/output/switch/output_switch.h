#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/output/binary_output.h"

namespace esphome {
namespace output {

class OutputSwitch : public switch_::Switch, public Component {
 public:
  void set_output(BinaryOutput *output) { output_ = output; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }
  void dump_config() override;

 protected:
  void write_state(bool state) override;

  output::BinaryOutput *output_;
};

}  // namespace output
}  // namespace esphome
