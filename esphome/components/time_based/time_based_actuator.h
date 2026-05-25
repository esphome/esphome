#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/actuator/actuator.h"

namespace esphome::time_based {

// Inheritance: TimeBasedActuatorBase -> Component (holds IActuator* pointer, no diamond)
class TimeBasedActuatorBase : public Component {
 public:
  void set_actuator(actuator::IActuator *actuator) { this->actuator_ = actuator; }
  void setup() override {}
  void loop() override;

  Trigger<> *get_open_trigger() { return &this->open_trigger_; }
  Trigger<> *get_close_trigger() { return &this->close_trigger_; }
  Trigger<> *get_stop_trigger() { return &this->stop_trigger_; }
  void set_open_duration(uint32_t open_duration) { this->open_duration_ = open_duration; }
  void set_close_duration(uint32_t close_duration) { this->close_duration_ = close_duration; }
  void set_has_built_in_endstop(bool value) { this->has_built_in_endstop_ = value; }
  void set_manual_control(bool value) { this->manual_control_ = value; }
  void set_assumed_state(bool value) { this->assumed_state_ = value; }
  bool get_assumed_state() const { return this->assumed_state_; }

 protected:
  void control(const actuator::ActuatorCallBase &call);
  void stop_prev_trigger_();
  bool is_at_target_() const;
  void start_direction_(actuator::ActuatorOperation dir);
  void recompute_position_();

  actuator::IActuator *actuator_{nullptr};
  Trigger<> open_trigger_;
  uint32_t open_duration_{0};
  Trigger<> close_trigger_;
  uint32_t close_duration_{0};
  Trigger<> stop_trigger_;

  Trigger<> *prev_command_trigger_{nullptr};
  uint32_t last_recompute_time_{0};
  uint32_t start_dir_time_{0};
  uint32_t last_publish_time_{0};
  float target_position_{0};
  bool has_built_in_endstop_{false};
  bool manual_control_{false};
  bool assumed_state_{false};
  actuator::ActuatorOperation last_operation_{actuator::ACTUATOR_OPERATION_OPENING};
};

}  // namespace esphome::time_based
