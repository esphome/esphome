#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/fan/fan_state.h"

namespace esphome {
namespace speed {

class SpeedFan : public Component {
 public:
  SpeedFan(fan::FanState *fan, output::FloatOutput *output, int speed_count)
      : fan_(fan), output_(output), speed_count_(speed_count) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_oscillating(output::BinaryOutput *oscillating) { this->oscillating_ = oscillating; }
  void set_direction(output::BinaryOutput *direction) { this->direction_ = direction; }

 protected:
  fan::FanState *fan_;
  output::FloatOutput *output_;
  output::BinaryOutput *oscillating_{nullptr};
  output::BinaryOutput *direction_{nullptr};
  bool next_update_{true};
  int speed_count_{};
};

}  // namespace speed
}  // namespace esphome
