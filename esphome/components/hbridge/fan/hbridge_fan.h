#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/fan/fan_state.h"

namespace esphome {
namespace hbridge {

enum DecayMode {
  DECAY_MODE_SLOW = 0,
  DECAY_MODE_FAST = 1,
};

class HBridgeFan : public Component {
 public:
  HBridgeFan(fan::FanState *fan, output::FloatOutput *pin_a, output::FloatOutput *pin_b, int speed_count,
             DecayMode decay_mode)
      : fan_(fan), pin_a_(pin_a), pin_b_(pin_b), speed_count_(speed_count), decay_mode_(decay_mode) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  fan::FanState *fan_;
  output::FloatOutput *pin_a_;
  output::FloatOutput *pin_b_;
  output::BinaryOutput *oscillating_{nullptr};
  bool next_update_{true};
  int speed_count_{};
  DecayMode decay_mode_{DECAY_MODE_SLOW};
};

}  // namespace hbridge
}  // namespace esphome
