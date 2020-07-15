#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/fan/fan_state.h"

namespace esphome {
namespace speed {

class SpeedFan : public Component {
 public:
  SpeedFan(fan::FanState *fan, output::FloatOutput *output) : fan_(fan), output_(output) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_oscillating(output::BinaryOutput *oscillating) { this->oscillating_ = oscillating; }
  void set_direction(output::BinaryOutput *direction) { this->direction_ = direction; }
  void set_speeds(float low, float medium, float high) {
    this->low_speed_ = low;
    this->medium_speed_ = medium;
    this->high_speed_ = high;
  }

 protected:
  fan::FanState *fan_;
  output::FloatOutput *output_;
  output::BinaryOutput *oscillating_{nullptr};
  output::BinaryOutput *direction_{nullptr};
  float low_speed_{};
  float medium_speed_{};
  float high_speed_{};
  bool next_update_{true};
};

}  // namespace speed
}  // namespace esphome
