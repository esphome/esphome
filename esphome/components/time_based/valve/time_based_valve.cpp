#include "time_based_valve.h"

namespace esphome::time_based {

using namespace esphome::valve;

static const char *const TAG = "time_based.valve";

void TimeBasedValve::setup() {
  this->reset_position();
  switch (this->restore_mode_) {
    case VALVE_NO_RESTORE:
      break;
    case VALVE_RESTORE: {
      auto restore = this->restore_state_();
      if (restore.has_value())
        restore->apply(this);
      break;
    }
    case VALVE_ALWAYS_OPEN:
      this->start_direction_(VALVE_OPERATION_OPENING);
      break;
    case VALVE_ALWAYS_CLOSED:
      this->start_direction_(VALVE_OPERATION_CLOSING);
      break;
  }
}

void TimeBasedValve::loop() {
  if (this->current_operation == VALVE_OPERATION_IDLE)
    return;

  const uint32_t now = App.get_loop_component_start_time();

  // Recompute position every loop cycle
  this->recompute_position_();

  if (this->is_at_target_()) {
    this->start_direction_(VALVE_OPERATION_IDLE);
    this->publish_state();
  }

  // Send current position every second
  if (now - this->last_publish_time_ > 1000) {
    this->publish_state(false);
    this->last_publish_time_ = now;

    ESP_LOGV(TAG, "Pos: %.2f Min: %.2f Max: %.2f", this->measured_position_, this->measured_position_min_,
             this->measured_position_max_);
  }
}

Trigger<> *TimeBasedValve::get_open_trigger() { return &this->open_trigger_; }
Trigger<> *TimeBasedValve::get_close_trigger() { return &this->close_trigger_; }
Trigger<> *TimeBasedValve::get_stop_trigger() { return &this->stop_trigger_; }

void TimeBasedValve::dump_config() {
  LOG_VALVE("", "Time Based Valve", this);
  ESP_LOGCONFIG(TAG, "  Duration: %.1fs", this->duration_ / 1e3f);
}

ValveTraits TimeBasedValve::get_traits() {
  auto traits = ValveTraits();
  traits.set_is_assumed_state(true);
  traits.set_supports_stop(true);
  traits.set_supports_toggle(true);
  traits.set_supports_position(true);
  return traits;
}

void TimeBasedValve::reset_position() {
  this->position = NAN;
  this->measured_position_ = 0;
  this->measured_position_min_ = 0;
  this->measured_position_max_ = 0;
}

void TimeBasedValve::set_position(float position, bool relative) {
  ValveOperation op;

  if (relative) {
    op = position < 0 ? VALVE_OPERATION_CLOSING : VALVE_OPERATION_OPENING;
    if (op == VALVE_OPERATION_CLOSING && this->position == VALVE_CLOSED ||
        op == VALVE_OPERATION_OPENING && this->position == VALVE_OPEN)
      return;
  } else {
    if (position == this->position)
      return;
    if (std::isnan(this->position)) {
      // If current position is unknown, only full open and close are possible
      if (position != VALVE_CLOSED && position != VALVE_OPEN)
        return;
      op = position == VALVE_CLOSED ? VALVE_OPERATION_CLOSING : VALVE_OPERATION_OPENING;
    } else {
      op = position < this->position ? VALVE_OPERATION_CLOSING : VALVE_OPERATION_OPENING;
    }
  }

  if (this->current_operation != VALVE_OPERATION_IDLE && this->current_operation != op) {
    // Stop before direction change
    this->start_direction_(VALVE_OPERATION_IDLE);
  }

  this->target_position_relative_ = relative;
  this->target_position_ = relative ? this->measured_position_ + position : position;
  this->start_direction_(op);
}

void TimeBasedValve::control(const ValveCall &call) {
  if (call.get_stop()) {
    this->start_direction_(VALVE_OPERATION_IDLE);
    this->publish_state();
  }
  if (call.get_toggle().has_value()) {
    if (this->current_operation != VALVE_OPERATION_IDLE) {
      this->start_direction_(VALVE_OPERATION_IDLE);
      this->publish_state();
    } else {
      if (this->position == VALVE_CLOSED || this->last_operation_ == VALVE_OPERATION_CLOSING) {
        this->set_position(VALVE_OPEN);
      } else {
        this->set_position(VALVE_CLOSED);
      }
    }
  }
  auto pos_val = call.get_position();
  if (pos_val.has_value()) {
    auto pos = *pos_val;
    this->set_position(pos);
  }
}

void TimeBasedValve::stop_prev_trigger_() {
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}

void TimeBasedValve::start_direction_(ValveOperation dir) {
  if (dir == this->current_operation && dir != VALVE_OPERATION_IDLE)
    return;

  this->recompute_position_();
  Trigger<> *trig;
  switch (dir) {
    case VALVE_OPERATION_IDLE:
      trig = &this->stop_trigger_;
      break;
    case VALVE_OPERATION_OPENING:
      this->last_operation_ = dir;
      trig = &this->open_trigger_;
      break;
    case VALVE_OPERATION_CLOSING:
      this->last_operation_ = dir;
      trig = &this->close_trigger_;
      break;
    default:
      return;
  }

  this->current_operation = dir;

  const uint32_t now = this->get_millis();
  this->last_recompute_time_ = now;

  this->stop_prev_trigger_();
  trig->trigger();
  this->prev_command_trigger_ = trig;
}

bool TimeBasedValve::is_at_target_() const {
  float position = this->target_position_relative_ ? this->measured_position_ : this->position;
  switch (this->current_operation) {
    case VALVE_OPERATION_OPENING:
      return position >= this->target_position_ || this->position == VALVE_OPEN;
    case VALVE_OPERATION_CLOSING:
      return position <= this->target_position_ || this->position == VALVE_CLOSED;
    case VALVE_OPERATION_IDLE:
    default:
      return true;
  }
}

void TimeBasedValve::recompute_position_() {
  if (this->current_operation == VALVE_OPERATION_IDLE)
    return;

  float dir;
  switch (this->current_operation) {
    case VALVE_OPERATION_OPENING:
      dir = 1.0f;
      break;
    case VALVE_OPERATION_CLOSING:
      dir = -1.0f;
      break;
    default:
      return;
  }

  const uint32_t now = this->get_millis();
  float distance = dir * (now - this->last_recompute_time_) / this->duration_;
  this->last_recompute_time_ = now;

  this->measured_position_ += distance;
  this->measured_position_ = clamp(this->measured_position_, -1.0f, 1.0f);
  if (this->measured_position_ > this->measured_position_max_) {
    this->measured_position_max_ = this->measured_position_;
  } else if (this->measured_position_ < this->measured_position_min_) {
    this->measured_position_min_ = this->measured_position_;
  }
  bool endstop_reached = (this->measured_position_max_ - this->measured_position_min_) >= 1.0f;

  if (endstop_reached && std::isnan(this->position)) {
    // Full duration traveled -> position is now known
    this->position = dir > 0 ? VALVE_OPEN : VALVE_CLOSED;
    return;
  }
  if (std::isnan(this->position))
    return;

  this->position += distance;
  if (endstop_reached) {
    this->position = clamp(this->position, 0.0f, 1.0f);
  } else {
    // Full duration not traveled yet -> keep value below target
    this->position = clamp(this->position, 0.01f, 0.99f);
  }
}

}  // namespace esphome::time_based
