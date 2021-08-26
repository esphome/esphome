#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/fan/fan_state.h"

namespace esphome {
namespace hbridge {

enum DecayMode {
  DECAY_MODE_SLOW = 0,
  DECAY_MODE_FAST = 1,
};

class HBridgeFan : public fan::FanState {
 public:
  HBridgeFan(int speed_count, DecayMode decay_mode) : speed_count_(speed_count), decay_mode_(decay_mode) {}

  void set_pin_a(output::FloatOutput *pin_a) { pin_a_ = pin_a; }
  void set_pin_b(output::FloatOutput *pin_b) { pin_b_ = pin_b; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  fan::FanStateCall brake();

 protected:
  output::FloatOutput *pin_a_;
  output::FloatOutput *pin_b_;
  output::BinaryOutput *oscillating_{nullptr};
  bool next_update_{true};
  int speed_count_{};
  DecayMode decay_mode_{DECAY_MODE_SLOW};
};

template<typename... Ts> class BrakeAction : public Action<Ts...> {
 public:
  explicit BrakeAction(HBridgeFan *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->brake(); }

  HBridgeFan *parent_;
};

}  // namespace hbridge
}  // namespace esphome
