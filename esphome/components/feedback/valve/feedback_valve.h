#pragma once

#include "../feedback_actuator.h"
#include "esphome/components/valve/valve.h"

namespace esphome::feedback {

// Inheritance: FeedbackValve -> FeedbackActuatorBase (Component), Valve (ActuatorBase, IActuator)
// No shared ancestors. No diamond inheritance.
class FeedbackValve : public FeedbackActuatorBase, public valve::Valve {
 public:
  FeedbackValve() { this->set_actuator(this); }
  valve::ValveTraits get_traits() override;
  void dump_config() override;

 protected:
  void control(const valve::ValveCall &call) override { FeedbackActuatorBase::control_(call); }
};

}  // namespace esphome::feedback
