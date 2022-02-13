#pragma once

#include "esphome/core/component.h"
#include "fan.h"

namespace esphome {
namespace fan {

enum ESPDEPRECATED("LegacyFanDirection members are deprecated, use FanDirection instead.",
                   "2022.2") LegacyFanDirection {
  FAN_DIRECTION_FORWARD = 0,
  FAN_DIRECTION_REVERSE = 1
};

class ESPDEPRECATED("FanState is deprecated, use Fan instead.", "2022.2") FanState : public Fan, public Component {
 public:
  FanState() = default;
  explicit FanState(const std::string &name) : Fan(name) {}

  /// Get the traits of this fan.
  FanTraits get_traits() override { return this->traits_; }
  /// Set the traits of this fan (i.e. what features it supports).
  void set_traits(const FanTraits &traits) { this->traits_ = traits; }

  void setup() override;
  float get_setup_priority() const override;

 protected:
  void control(const FanCall &call) override { this->publish_state(); }

  FanTraits traits_{};
};

}  // namespace fan
}  // namespace esphome
