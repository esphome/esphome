#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "esphome/components/actuator/actuator.h"

namespace esphome::feedback {

// Inheritance: FeedbackActuatorBase -> Component (holds IActuator* pointer, no diamond)
class FeedbackActuatorBase : public Component {
 public:
  void set_actuator(actuator::IActuator *actuator) { this->actuator_ = actuator; }
  void setup() override;
  void loop() override;

  Trigger<> *get_open_trigger() { return &this->open_trigger_; }
  Trigger<> *get_close_trigger() { return &this->close_trigger_; }
  Trigger<> *get_stop_trigger() { return &this->stop_trigger_; }

#ifdef USE_BINARY_SENSOR
  void set_open_endstop(binary_sensor::BinarySensor *open_endstop);
  void set_open_sensor(binary_sensor::BinarySensor *open_feedback);
  void set_open_obstacle_sensor(binary_sensor::BinarySensor *open_obstacle);
  void set_close_endstop(binary_sensor::BinarySensor *close_endstop);
  void set_close_sensor(binary_sensor::BinarySensor *close_feedback);
  void set_close_obstacle_sensor(binary_sensor::BinarySensor *close_obstacle);
#endif
  void set_open_duration(uint32_t duration) { this->open_duration_ = duration; }
  void set_close_duration(uint32_t duration) { this->close_duration_ = duration; }
  void set_has_built_in_endstop(bool value) { this->has_built_in_endstop_ = value; }
  void set_assumed_state(bool value) { this->assumed_state_ = value; }
  bool get_assumed_state() const { return this->assumed_state_; }
  bool get_has_built_in_endstop() const { return this->has_built_in_endstop_; }
  bool get_infer_endstop() const { return this->infer_endstop_; }
  uint32_t get_open_duration() const { return this->open_duration_; }
  uint32_t get_close_duration() const { return this->close_duration_; }
  uint32_t get_max_duration() const { return this->max_duration_; }
  const optional<uint32_t> &get_direction_change_waittime() const { return this->direction_change_waittime_; }
  uint32_t get_acceleration_wait_time() const { return this->acceleration_wait_time_; }
  float get_obstacle_rollback() const { return this->obstacle_rollback_; }
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *get_open_endstop() const { return this->open_endstop_; }
  binary_sensor::BinarySensor *get_close_endstop() const { return this->close_endstop_; }
  binary_sensor::BinarySensor *get_open_feedback() const { return this->open_feedback_; }
  binary_sensor::BinarySensor *get_close_feedback() const { return this->close_feedback_; }
  binary_sensor::BinarySensor *get_open_obstacle() const { return this->open_obstacle_; }
  binary_sensor::BinarySensor *get_close_obstacle() const { return this->close_obstacle_; }
#endif
  void set_max_duration(uint32_t max_duration) { this->max_duration_ = max_duration; }
  void set_obstacle_rollback(float obstacle_rollback) { this->obstacle_rollback_ = obstacle_rollback; }
  void set_update_interval(uint32_t interval) { this->update_interval_ = interval; }
  void set_infer_endstop(bool infer_endstop) { this->infer_endstop_ = infer_endstop; }
  void set_direction_change_waittime(uint32_t waittime) { this->direction_change_waittime_ = waittime; }
  void set_acceleration_wait_time(uint32_t waittime) { this->acceleration_wait_time_ = waittime; }

 protected:
  void control_(const actuator::ActuatorCallBase &call);
  void stop_prev_trigger_();
  bool is_at_target_() const;
  void start_direction_(actuator::ActuatorOperation dir);
  void endstop_reached_(bool open_endstop);
  void recompute_position_();
  void set_current_operation_(actuator::ActuatorOperation operation, bool is_triggered);

  actuator::IActuator *actuator_{nullptr};
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *open_endstop_{nullptr};
  binary_sensor::BinarySensor *close_endstop_{nullptr};
  binary_sensor::BinarySensor *open_feedback_{nullptr};
  binary_sensor::BinarySensor *close_feedback_{nullptr};
  binary_sensor::BinarySensor *open_obstacle_{nullptr};
  binary_sensor::BinarySensor *close_obstacle_{nullptr};
#endif
  Trigger<> open_trigger_;
  Trigger<> close_trigger_;
  Trigger<> stop_trigger_;

  uint32_t open_duration_{0};
  uint32_t close_duration_{0};
  uint32_t max_duration_{UINT32_MAX};
  optional<uint32_t> direction_change_waittime_{};
  uint32_t acceleration_wait_time_{0};
  bool has_built_in_endstop_{false};
  bool assumed_state_{false};
  bool infer_endstop_{false};
  float obstacle_rollback_{0};

  actuator::ActuatorOperation last_operation_{actuator::ACTUATOR_OPERATION_OPENING};
  actuator::ActuatorOperation current_trigger_operation_{actuator::ACTUATOR_OPERATION_IDLE};
  Trigger<> *prev_command_trigger_{nullptr};
  uint32_t last_recompute_time_{0};
  uint32_t start_dir_time_{0};
  uint32_t last_publish_time_{0};
  float target_position_{0};
  uint32_t update_interval_{1000};
};

}  // namespace esphome::feedback
