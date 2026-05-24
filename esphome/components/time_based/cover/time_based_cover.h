#pragma once

#include "esphome/components/actuator/time_based_actuator.h"
#include "esphome/components/cover/cover.h"

namespace esphome::time_based {

// Inheritance: TimeBasedCover -> TimeBasedActuatorBase (Component), Cover (ActuatorBase, IActuator)
// No shared ancestors. No diamond inheritance.
class TimeBasedCover : public actuator::TimeBasedActuatorBase, public cover::Cover {
 public:
  TimeBasedCover() { this->set_actuator(this); }
  void setup() override;
  cover::CoverTraits get_traits() override;
  void dump_config() override;

 protected:
  void control(const cover::CoverCall &call) override { actuator::TimeBasedActuatorBase::control(call); }
};

}  // namespace esphome::time_based
