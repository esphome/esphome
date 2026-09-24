#include "endstop_actuator.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

namespace esphome::endstop {

using namespace esphome::actuator;

static const char *const TAG = "endstop";

void EndstopActuatorBase::control_(const ActuatorCallBase &call) {
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
  auto opt_pos = call.get_position();
  if (opt_pos.has_value()) {
    auto pos = *opt_pos;
    if (pos == this->actuator_->get_position()) {
      // already at target
    } else {
      auto op = pos < this->actuator_->get_position() ? ACTUATOR_OPERATION_CLOSING : ACTUATOR_OPERATION_OPENING;
      this->target_position_ = pos;
      this->start_direction_(op);
    }
  }
}

void EndstopActuatorBase::setup() {
  auto restored_pos = this->actuator_->do_restore_state();

  if (this->is_open_()) {
    this->actuator_->set_position(ACTUATOR_OPEN);
  } else if (this->is_closed_()) {
    this->actuator_->set_position(ACTUATOR_CLOSED);
  } else if (!restored_pos.has_value()) {
    this->actuator_->set_position(0.5f);
  }
}

void EndstopActuatorBase::loop() {
  if (this->actuator_->get_operation() == ACTUATOR_OPERATION_IDLE)
    return;

  const uint32_t now = App.get_loop_component_start_time();

  if (this->actuator_->get_operation() == ACTUATOR_OPERATION_OPENING && this->is_open_()) {
    float dur = (now - this->start_dir_time_) / 1e3f;
    ESP_LOGD(TAG, "'%s' - Open endstop reached. Took %.1fs.", this->actuator_->get_entity_name(), dur);

    this->start_direction_(ACTUATOR_OPERATION_IDLE);
    this->actuator_->set_position(ACTUATOR_OPEN);
    this->actuator_->do_publish_state(true);
  } else if (this->actuator_->get_operation() == ACTUATOR_OPERATION_CLOSING && this->is_closed_()) {
    float dur = (now - this->start_dir_time_) / 1e3f;
    ESP_LOGD(TAG, "'%s' - Close endstop reached. Took %.1fs.", this->actuator_->get_entity_name(), dur);

    this->start_direction_(ACTUATOR_OPERATION_IDLE);
    this->actuator_->set_position(ACTUATOR_CLOSED);
    this->actuator_->do_publish_state(true);
  } else if (now - this->start_dir_time_ > this->max_duration_) {
    ESP_LOGD(TAG, "'%s' - Max duration reached. Stopping.", this->actuator_->get_entity_name());
    this->start_direction_(ACTUATOR_OPERATION_IDLE);
    this->actuator_->do_publish_state(true);
  }

  // Recompute position every loop cycle
  this->recompute_position_();

  if (this->actuator_->get_operation() != ACTUATOR_OPERATION_IDLE && this->is_at_target_()) {
    this->start_direction_(ACTUATOR_OPERATION_IDLE);
    this->actuator_->do_publish_state(true);
  }

  // Send current position every second
  if (this->actuator_->get_operation() != ACTUATOR_OPERATION_IDLE && now - this->last_publish_time_ > 1000) {
    this->actuator_->do_publish_state(false);
    this->last_publish_time_ = now;
  }
}

void EndstopActuatorBase::stop_prev_trigger_() {
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}

bool EndstopActuatorBase::is_at_target_() const {
  switch (this->actuator_->get_operation()) {
    case ACTUATOR_OPERATION_OPENING:
      if (this->target_position_ == ACTUATOR_OPEN)
        return this->is_open_();
      return this->actuator_->get_position() >= this->target_position_;
    case ACTUATOR_OPERATION_CLOSING:
      if (this->target_position_ == ACTUATOR_CLOSED)
        return this->is_closed_();
      return this->actuator_->get_position() <= this->target_position_;
    case ACTUATOR_OPERATION_IDLE:
    default:
      return true;
  }
}

void EndstopActuatorBase::start_direction_(ActuatorOperation dir) {
  if (dir == this->actuator_->get_operation())
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

  this->stop_prev_trigger_();
  trig->trigger();
  this->prev_command_trigger_ = trig;

  const uint32_t now = millis();
  this->start_dir_time_ = now;
  this->last_recompute_time_ = now;
}

void EndstopActuatorBase::recompute_position_() {
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

}  // namespace esphome::endstop
