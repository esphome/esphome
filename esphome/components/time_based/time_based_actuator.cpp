#include "time_based_actuator.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::time_based {

using namespace esphome::actuator;

void TimeBasedActuatorBase::loop() {
  if (this->actuator_->get_operation() == ACTUATOR_OPERATION_IDLE)
    return;

  const uint32_t now = App.get_loop_component_start_time();

  // Recompute position every loop cycle
  this->recompute_position_();

  if (this->is_at_target_()) {
    if (this->has_built_in_endstop_ &&
        (this->target_position_ == ACTUATOR_OPEN || this->target_position_ == ACTUATOR_CLOSED)) {
      // Don't trigger stop, let the actuator stop by itself.
      this->actuator_->set_operation(ACTUATOR_OPERATION_IDLE);
    } else {
      this->start_direction_(ACTUATOR_OPERATION_IDLE);
    }
    this->actuator_->do_publish_state(true);
  }

  // Send current position every second
  if (now - this->last_publish_time_ > 1000) {
    this->actuator_->do_publish_state(false);
    this->last_publish_time_ = now;
  }
}

void TimeBasedActuatorBase::control(const ActuatorCallBase &call) {
  if (call.get_stop()) {
    this->start_direction_(ACTUATOR_OPERATION_IDLE);
    this->actuator_->do_publish_state(true);
  }
  if (call.get_toggle().has_value()) {
    if (this->actuator_->get_operation() != ACTUATOR_OPERATION_IDLE) {
      this->start_direction_(ACTUATOR_OPERATION_IDLE);
      this->actuator_->do_publish_state(true);
    } else {
      if (this->actuator_->get_position() == ACTUATOR_CLOSED || this->last_operation_ == ACTUATOR_OPERATION_CLOSING) {
        this->target_position_ = ACTUATOR_OPEN;
        this->start_direction_(ACTUATOR_OPERATION_OPENING);
      } else {
        this->target_position_ = ACTUATOR_CLOSED;
        this->start_direction_(ACTUATOR_OPERATION_CLOSING);
      }
    }
  }
  auto pos_val = call.get_position();
  if (pos_val.has_value()) {
    auto pos = *pos_val;
    if (pos == this->actuator_->get_position()) {
      // already at target
      if (this->manual_control_ && (pos == ACTUATOR_OPEN || pos == ACTUATOR_CLOSED)) {
        // for actuators with manual control switch, we can't rely on the computed position, so if
        // the command triggered again, we'll assume it's in the opposite direction anyway.
        auto op = pos == ACTUATOR_CLOSED ? ACTUATOR_OPERATION_CLOSING : ACTUATOR_OPERATION_OPENING;
        this->actuator_->set_position(pos == ACTUATOR_CLOSED ? ACTUATOR_OPEN : ACTUATOR_CLOSED);
        this->target_position_ = pos;
        this->start_direction_(op);
      }
      // for actuators with built in end stop, we should send the command again
      if (this->has_built_in_endstop_ && (pos == ACTUATOR_OPEN || pos == ACTUATOR_CLOSED)) {
        auto op = pos == ACTUATOR_CLOSED ? ACTUATOR_OPERATION_CLOSING : ACTUATOR_OPERATION_OPENING;
        this->target_position_ = pos;
        this->start_direction_(op);
      }
    } else {
      auto op = pos < this->actuator_->get_position() ? ACTUATOR_OPERATION_CLOSING : ACTUATOR_OPERATION_OPENING;
      if (this->manual_control_ && (pos == ACTUATOR_OPEN || pos == ACTUATOR_CLOSED)) {
        this->actuator_->set_position(pos == ACTUATOR_CLOSED ? ACTUATOR_OPEN : ACTUATOR_CLOSED);
      }
      this->target_position_ = pos;
      this->start_direction_(op);
    }
  }
}

void TimeBasedActuatorBase::stop_prev_trigger_() {
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}

bool TimeBasedActuatorBase::is_at_target_() const {
  switch (this->actuator_->get_operation()) {
    case ACTUATOR_OPERATION_OPENING:
      return this->actuator_->get_position() >= this->target_position_;
    case ACTUATOR_OPERATION_CLOSING:
      return this->actuator_->get_position() <= this->target_position_;
    case ACTUATOR_OPERATION_IDLE:
    default:
      return true;
  }
}

void TimeBasedActuatorBase::start_direction_(ActuatorOperation dir) {
  if (dir == this->actuator_->get_operation() && dir != ACTUATOR_OPERATION_IDLE)
    return;

  this->recompute_position_();
  Trigger<> *trig;
  switch (dir) {
    case ACTUATOR_OPERATION_IDLE:
      trig = &this->stop_trigger_;
      break;
    case ACTUATOR_OPERATION_OPENING:
      this->last_operation_ = dir;
      trig = &this->open_trigger_;
      break;
    case ACTUATOR_OPERATION_CLOSING:
      this->last_operation_ = dir;
      trig = &this->close_trigger_;
      break;
    default:
      return;
  }

  this->actuator_->set_operation(dir);

  const uint32_t now = millis();
  this->start_dir_time_ = now;
  this->last_recompute_time_ = now;

  this->stop_prev_trigger_();
  trig->trigger();
  this->prev_command_trigger_ = trig;
}

void TimeBasedActuatorBase::recompute_position_() {
  if (this->actuator_->get_operation() == ACTUATOR_OPERATION_IDLE)
    return;

  float dir;
  float action_dur;
  switch (this->actuator_->get_operation()) {
    case ACTUATOR_OPERATION_OPENING:
      dir = 1.0f;
      action_dur = this->open_duration_;
      break;
    case ACTUATOR_OPERATION_CLOSING:
      dir = -1.0f;
      action_dur = this->close_duration_;
      break;
    default:
      return;
  }

  const uint32_t now = millis();
  float new_pos = this->actuator_->get_position() + dir * (now - this->last_recompute_time_) / action_dur;
  this->actuator_->set_position(clamp(new_pos, 0.0f, 1.0f));

  this->last_recompute_time_ = now;
}

}  // namespace esphome::time_based
