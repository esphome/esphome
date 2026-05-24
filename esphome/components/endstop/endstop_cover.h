#pragma once

#include "esphome/components/actuator/endstop_actuator.h"
#include "esphome/components/cover/cover.h"

namespace esphome::endstop {

// Inheritance: EndstopCover -> EndstopActuatorBase (Component), Cover (ActuatorBase, IActuator)
// No shared ancestors. No diamond inheritance.
class EndstopCover : public actuator::EndstopActuatorBase, public cover::Cover {
 public:
  EndstopCover() { this->set_actuator(this); }
  cover::CoverTraits get_traits() override;
  void dump_config() override;

 protected:
  void control(const cover::CoverCall &call) override { actuator::EndstopActuatorBase::control(call); }
};

}  // namespace esphome::endstop
