#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace binary {

class BinaryFan : public Component, public fan::Fan {
 public:
  void setup() override;
  void dump_config() override;

  void set_output(output::BinaryOutput *output) { this->output_ = output; }
  void set_oscillating(output::BinaryOutput *oscillating) { this->oscillating_ = oscillating; }
  void set_direction(output::BinaryOutput *direction) { this->direction_ = direction; }

  fan::FanTraits get_traits() override;

 protected:
  void control(const fan::FanCall &call) override;
  void write_state_();

  output::BinaryOutput *output_;
  output::BinaryOutput *oscillating_{nullptr};
  output::BinaryOutput *direction_{nullptr};
};

}  // namespace binary
}  // namespace esphome
