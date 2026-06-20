#pragma once

#include "../endstop_actuator.h"
#include "esphome/components/valve/valve.h"

namespace esphome::endstop {

// Inheritance: EndstopValve -> EndstopActuatorBase (Component), Valve (ActuatorBase, IActuator)
// No shared ancestors. No diamond inheritance.
class EndstopValve : public EndstopActuatorBase, public valve::Valve {
 public:
  EndstopValve() { this->set_actuator(this); }
  valve::ValveTraits get_traits() override;
  void dump_config() override;

 protected:
  void control(const valve::ValveCall &call) override { EndstopActuatorBase::control_(call); }
};

}  // namespace esphome::endstop
