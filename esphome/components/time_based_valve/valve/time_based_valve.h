#pragma once

#include "esphome/components/time_based/time_based_actuator.h"
#include "esphome/components/valve/valve.h"

namespace esphome::time_based_valve {

// Inheritance: TimeBasedValve -> TimeBasedActuatorBase (Component), Valve (ActuatorBase, IActuator)
// No shared ancestors. No diamond inheritance.
class TimeBasedValve : public time_based::TimeBasedActuatorBase, public valve::Valve {
 public:
  TimeBasedValve() { this->set_actuator(this); }
  void setup() override;
  valve::ValveTraits get_traits() override;
  void dump_config() override;

 protected:
  void control(const valve::ValveCall &call) override { time_based::TimeBasedActuatorBase::control(call); }
};

}  // namespace esphome::time_based_valve
