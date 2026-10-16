#pragma once

#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::endstop {

// Defined in endstop_actuator.cpp: the logging macros cannot be used in header files.
void log_endstop_reached(const char *tag, const char *name, bool open, uint32_t duration_ms);
void log_max_duration_reached(const char *tag, const char *name);

/** Endstop logic shared by the endstop cover and valve platforms.
 *
 * Types describes the entity the logic drives, and must provide:
 *  - Entity: the entity base class (cover::Cover or valve::Valve)
 *  - Call: the call type passed to Entity::control()
 *  - Operation: the entity's operation enum
 *  - IDLE, OPENING, CLOSING: the Operation values
 *  - TAG: the log tag
 *
 * The concrete platform class derives from EndstopActuator<Types> and implements get_traits() and dump_config().
 */
template<typename Types> class EndstopActuator : public Types::Entity, public Component {
 public:
  void setup() override;
  void loop() override;

  Trigger<> *get_open_trigger() { return &this->open_trigger_; }
  Trigger<> *get_close_trigger() { return &this->close_trigger_; }
  Trigger<> *get_stop_trigger() { return &this->stop_trigger_; }
  void set_open_endstop(binary_sensor::BinarySensor *open_endstop) { this->open_endstop_ = open_endstop; }
  void set_close_endstop(binary_sensor::BinarySensor *close_endstop) { this->close_endstop_ = close_endstop; }
  void set_open_duration(uint32_t open_duration) { this->open_duration_ = open_duration; }
  void set_close_duration(uint32_t close_duration) { this->close_duration_ = close_duration; }
  void set_max_duration(uint32_t max_duration) { this->max_duration_ = max_duration; }

 protected:
  using Operation = typename Types::Operation;
  static constexpr float POSITION_OPEN = 1.0f;
  static constexpr float POSITION_CLOSED = 0.0f;

  void control(const typename Types::Call &call) override;
  void stop_prev_trigger_();
  bool is_open_() const { return this->open_endstop_->state; }
  bool is_closed_() const { return this->close_endstop_->state; }
  bool is_at_target_() const;
  void start_direction_(Operation dir);
  void recompute_position_();

  binary_sensor::BinarySensor *open_endstop_;
  binary_sensor::BinarySensor *close_endstop_;
  Trigger<> open_trigger_;
  uint32_t open_duration_;
  Trigger<> close_trigger_;
  uint32_t close_duration_;
  Trigger<> stop_trigger_;
  uint32_t max_duration_{UINT32_MAX};

  Trigger<> *prev_command_trigger_{nullptr};
  uint32_t last_recompute_time_{0};
  uint32_t start_dir_time_{0};
  uint32_t last_publish_time_{0};
  float target_position_{0};
  Operation last_operation_{Types::OPENING};
};

template<typename Types> void EndstopActuator<Types>::control(const typename Types::Call &call) {
  if (call.get_stop()) {
    this->start_direction_(Types::IDLE);
    this->publish_state();
  }
  if (call.get_toggle().has_value()) {
    if (this->current_operation != Types::IDLE) {
      this->start_direction_(Types::IDLE);
      this->publish_state();
    } else {
      if (this->position == POSITION_CLOSED || this->last_operation_ == Types::CLOSING) {
        this->target_position_ = POSITION_OPEN;
        this->start_direction_(Types::OPENING);
      } else {
        this->target_position_ = POSITION_CLOSED;
        this->start_direction_(Types::CLOSING);
      }
    }
  }
  auto opt_pos = call.get_position();
  if (opt_pos.has_value()) {
    auto pos = *opt_pos;
    if (pos == this->position) {
      // already at target
    } else {
      auto op = pos < this->position ? Types::CLOSING : Types::OPENING;
      this->target_position_ = pos;
      this->start_direction_(op);
    }
  }
}

template<typename Types> void EndstopActuator<Types>::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  }

  if (this->is_open_()) {
    this->position = POSITION_OPEN;
  } else if (this->is_closed_()) {
    this->position = POSITION_CLOSED;
  } else if (!restore.has_value()) {
    this->position = 0.5f;
  }
}

template<typename Types> void EndstopActuator<Types>::loop() {
  if (this->current_operation == Types::IDLE)
    return;

  const uint32_t now = App.get_loop_component_start_time();

  if (this->current_operation == Types::OPENING && this->is_open_()) {
    log_endstop_reached(Types::TAG, this->get_name().c_str(), true, now - this->start_dir_time_);

    this->start_direction_(Types::IDLE);
    this->position = POSITION_OPEN;
    this->publish_state();
  } else if (this->current_operation == Types::CLOSING && this->is_closed_()) {
    log_endstop_reached(Types::TAG, this->get_name().c_str(), false, now - this->start_dir_time_);

    this->start_direction_(Types::IDLE);
    this->position = POSITION_CLOSED;
    this->publish_state();
  } else if (now - this->start_dir_time_ > this->max_duration_) {
    log_max_duration_reached(Types::TAG, this->get_name().c_str());
    this->start_direction_(Types::IDLE);
    this->publish_state();
  }

  // Recompute position every loop cycle
  this->recompute_position_();

  if (this->current_operation != Types::IDLE && this->is_at_target_()) {
    this->start_direction_(Types::IDLE);
    this->publish_state();
  }

  // Send current position every second
  if (this->current_operation != Types::IDLE && now - this->last_publish_time_ > 1000) {
    this->publish_state(false);
    this->last_publish_time_ = now;
  }
}

template<typename Types> void EndstopActuator<Types>::stop_prev_trigger_() {
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}

template<typename Types> bool EndstopActuator<Types>::is_at_target_() const {
  switch (this->current_operation) {
    case Types::OPENING:
      if (this->target_position_ == POSITION_OPEN)
        return this->is_open_();
      return this->position >= this->target_position_;
    case Types::CLOSING:
      if (this->target_position_ == POSITION_CLOSED)
        return this->is_closed_();
      return this->position <= this->target_position_;
    case Types::IDLE:
    default:
      return true;
  }
}

template<typename Types> void EndstopActuator<Types>::start_direction_(Operation dir) {
  if (dir == this->current_operation)
    return;

  this->recompute_position_();
  Trigger<> *trig;
  switch (dir) {
    case Types::IDLE:
      trig = &this->stop_trigger_;
      break;
    case Types::OPENING:
      this->last_operation_ = dir;
      trig = &this->open_trigger_;
      break;
    case Types::CLOSING:
      this->last_operation_ = dir;
      trig = &this->close_trigger_;
      break;
    default:
      return;
  }

  this->current_operation = dir;

  this->stop_prev_trigger_();
  trig->trigger();
  this->prev_command_trigger_ = trig;

  const uint32_t now = millis();
  this->start_dir_time_ = now;
  this->last_recompute_time_ = now;
}

template<typename Types> void EndstopActuator<Types>::recompute_position_() {
  if (this->current_operation == Types::IDLE)
    return;

  float dir;
  float action_dur;
  switch (this->current_operation) {
    case Types::OPENING:
      dir = 1.0f;
      action_dur = this->open_duration_;
      break;
    case Types::CLOSING:
      dir = -1.0f;
      action_dur = this->close_duration_;
      break;
    default:
      return;
  }

  const uint32_t now = millis();
  this->position += dir * (now - this->last_recompute_time_) / action_dur;
  this->position = clamp(this->position, 0.0f, 1.0f);

  this->last_recompute_time_ = now;
}

}  // namespace esphome::endstop
