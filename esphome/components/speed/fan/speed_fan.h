#pragma once

#include <unordered_map>

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace speed {

class SpeedFan : public Component, public fan::Fan {
 public:
  SpeedFan(output::FloatOutput *output, int speed_count) : output_(output), speed_count_(speed_count) {}
  void setup() override;
  void dump_config() override;
  void set_oscillating(output::BinaryOutput *oscillating) { this->oscillating_ = oscillating; }
  void set_direction(output::BinaryOutput *direction) { this->direction_ = direction; }
  void add_preset_mode(const std::string &name, optional<int> speed, optional<bool> oscillating,
                       optional<fan::FanDirection> direction);
  fan::FanTraits get_traits() override { return this->traits_; }

 protected:
  void control(const fan::FanCall &call) override;
  void write_state_();

  output::FloatOutput *output_;
  output::BinaryOutput *oscillating_{nullptr};
  output::BinaryOutput *direction_{nullptr};
  int speed_count_{};
  fan::FanTraits traits_;
  std::unordered_map<std::string, const fan::FanCall> preset_modes_{};
};

}  // namespace speed
}  // namespace esphome
