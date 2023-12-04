#pragma once

#include <set>

#include "esphome/core/automation.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace hbridge {

enum DecayMode {
  DECAY_MODE_SLOW = 0,
  DECAY_MODE_FAST = 1,
};

class HBridgeFan : public Component, public fan::Fan {
 public:
  HBridgeFan(int speed_count, DecayMode decay_mode) : speed_count_(speed_count), decay_mode_(decay_mode) {}

  void set_pin_a(output::FloatOutput *pin_a) { pin_a_ = pin_a; }
  void set_pin_b(output::FloatOutput *pin_b) { pin_b_ = pin_b; }
  void set_enable_pin(output::FloatOutput *enable) { enable_ = enable; }
  void set_preset_modes(const std::set<std::string> &presets) { preset_modes_ = presets; }

  void setup() override;
  void dump_config() override;
  fan::FanTraits get_traits() override { return this->traits_; }

  fan::FanCall brake();

 protected:
  output::FloatOutput *pin_a_;
  output::FloatOutput *pin_b_;
  output::FloatOutput *enable_{nullptr};
  output::BinaryOutput *oscillating_{nullptr};
  int speed_count_{};
  DecayMode decay_mode_{DECAY_MODE_SLOW};
  fan::FanTraits traits_;
  std::set<std::string> preset_modes_{};

  void control(const fan::FanCall &call) override;
  void write_state_();

  void set_hbridge_levels_(float a_level, float b_level);
  void set_hbridge_levels_(float a_level, float b_level, float enable);
};

template<typename... Ts> class BrakeAction : public Action<Ts...> {
 public:
  explicit BrakeAction(HBridgeFan *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->brake(); }

  HBridgeFan *parent_;
};

}  // namespace hbridge
}  // namespace esphome
