#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/hbridge/hbridge.h"

namespace esphome {
namespace hbridge {


class HBridgeFan : public hbridge::HBridge, public fan::Fan {

 public:
  HBridgeFan(int speed_count) : speed_count_(speed_count) {}

  //Config set functions
  void set_oscillation_output(output::BinaryOutput * output) { this->oscillating_output_ = output; }

  //Component interfacing
  void setup() override;
  void dump_config() override;
  fan::FanTraits get_traits() override;

  fan::FanCall brake();

 protected:
  output::BinaryOutput *oscillating_{nullptr};
  int speed_count_{};

  void control(const fan::FanCall &call) override;
  void write_state_();
};

//Action template
template<typename... Ts> class BrakeAction : public Action<Ts...> {
 public:
  explicit BrakeAction(HBridgeFan *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->brake(); }

  HBridgeFan *parent_;
};

}  // namespace hbridge
}  // namespace esphome
