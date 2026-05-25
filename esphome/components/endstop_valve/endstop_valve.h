#pragma once

#include "esphome/components/endstop/endstop_actuator.h"
#include "esphome/components/valve/valve.h"

namespace esphome::endstop_valve {

// Inheritance: EndstopValve -> EndstopActuatorBase (Component), Valve (ActuatorBase, IActuator)
// No shared ancestors. No diamond inheritance.
class EndstopValve : public endstop::EndstopActuatorBase, public valve::Valve {
 public:
  EndstopValve() { this->set_actuator(this); }
  valve::ValveTraits get_traits() override;
  void dump_config() override;

 protected:
  void control(const valve::ValveCall &call) override { endstop::EndstopActuatorBase::control(call); }
};

}  // namespace esphome::endstop_valve
