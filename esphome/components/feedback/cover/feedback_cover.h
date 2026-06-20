#pragma once

#include "../feedback_actuator.h"
#include "esphome/components/cover/cover.h"

namespace esphome::feedback {

// Inheritance: FeedbackCover -> FeedbackActuatorBase (Component), Cover (ActuatorBase, IActuator)
// No shared ancestors. No diamond inheritance.
class FeedbackCover : public FeedbackActuatorBase, public cover::Cover {
 public:
  FeedbackCover() { this->set_actuator(this); }
  cover::CoverTraits get_traits() override;
  void dump_config() override;

 protected:
  void control(const cover::CoverCall &call) override { FeedbackActuatorBase::control_(call); }
};

}  // namespace esphome::feedback
