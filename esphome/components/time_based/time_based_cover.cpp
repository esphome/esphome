#include "time_based_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace time_based {

static const char *const TAG = "time_based.cover";

using namespace esphome::cover;

void TimeBasedCover::dump_config() {
  LOG_COVER("", "Time Based Cover", this);
  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
}
void TimeBasedCover::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->time_position_ = 0.5f;
  }
  this->translate_position_();
}
void TimeBasedCover::loop() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  const uint32_t now = millis();

  // Recompute position every loop cycle
  this->recompute_position_();

  if (this->is_at_target_()) {
    if (this->has_built_in_endstop_ &&
        (this->target_position_ == COVER_OPEN || this->target_position_ == COVER_CLOSED)) {
      // Don't trigger stop, let the cover stop by itself.
      this->current_operation = COVER_OPERATION_IDLE;
    } else {
      this->start_direction_(COVER_OPERATION_IDLE);
    }
    this->publish_state();
  }

  // Send current position every second
  if (now - this->last_publish_time_ > 1000) {
    this->publish_state(false);
    ESP_LOGD(TAG, "  Time position: %.0f%%", this->time_position_ * 100.0f);
    this->last_publish_time_ = now;
  }
}
float TimeBasedCover::get_setup_priority() const { return setup_priority::DATA; }
CoverTraits TimeBasedCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(this->assumed_state_);
  return traits;
}
void TimeBasedCover::control(const CoverCall &call) {
  if (call.get_stop()) {
    this->start_direction_(COVER_OPERATION_IDLE);
    this->publish_state();
  }
  if (call.get_toggle().has_value()) {
    if (this->current_operation != COVER_OPERATION_IDLE) {
      this->start_direction_(COVER_OPERATION_IDLE);
      this->publish_state();
    } else {
      if (this->position == COVER_CLOSED || this->last_operation_ == COVER_OPERATION_CLOSING) {
        this->target_position_ = COVER_OPEN;
        this->start_direction_(COVER_OPERATION_OPENING);
      } else {
        this->target_position_ = COVER_CLOSED;
        this->start_direction_(COVER_OPERATION_CLOSING);
      }
    }
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    if (pos == this->position) {
      // already at target
      // for covers with built in end stop, we should send the command again
      if (this->has_built_in_endstop_ && (pos == COVER_OPEN || pos == COVER_CLOSED)) {
        auto op = pos == COVER_CLOSED ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING;
        this->target_position_ = pos;
        this->start_direction_(op);
      }
    } else {
      auto op = pos < this->position ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING;
      this->target_position_ = pos;
      this->start_direction_(op);
    }
  }
}
void TimeBasedCover::stop_prev_trigger_() {
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}
bool TimeBasedCover::is_at_target_() const {
  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      return this->position >= this->target_position_;
    case COVER_OPERATION_CLOSING:
      return this->position <= this->target_position_;
    case COVER_OPERATION_IDLE:
    default:
      return true;
  }
}
void TimeBasedCover::start_direction_(CoverOperation dir) {
  if (dir == this->current_operation && dir != COVER_OPERATION_IDLE)
    return;

  this->recompute_position_();
  Trigger<> *trig;
  switch (dir) {
    case COVER_OPERATION_IDLE:
      trig = this->stop_trigger_;
      break;
    case COVER_OPERATION_OPENING:
      this->last_operation_ = dir;
      trig = this->open_trigger_;
      break;
    case COVER_OPERATION_CLOSING:
      this->last_operation_ = dir;
      trig = this->close_trigger_;
      break;
    default:
      return;
  }

  this->current_operation = dir;

  const uint32_t now = millis();
  this->start_dir_time_ = now;
  this->last_recompute_time_ = now;

  this->stop_prev_trigger_();
  trig->trigger();
  this->prev_command_trigger_ = trig;
}
void TimeBasedCover::recompute_position_() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  float dir;
  float action_dur;
  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      dir = 1.0f;
      action_dur = this->open_duration_;
      break;
    case COVER_OPERATION_CLOSING:
      dir = -1.0f;
      action_dur = this->close_duration_;
      break;
    default:
      return;
  }

  const uint32_t now = millis();
  this->time_position_ += dir * (now - this->last_recompute_time_) / action_dur;
  this->time_position_ = clamp(this->time_position_, 0.0f, 1.0f);
  this->translate_position_();

  this->last_recompute_time_ = now;
}

void TimeBasedCover::translate_position_() {
  if (this->piecewise_cuts_.empty()) {
    this->position = this->time_position_;
    return;
  }
  // Find which piece it is
  int piece = piecewise_cuts_.size() - 1;
  for (size_t i = 0; i < this->piecewise_cuts_.size(); i++) {
    if (this->time_position_ <= this->piecewise_cuts_[i]) {
      piece = i;
      break;
    }
  }

  // Retrieve the coefficients of the linear segment
  float m = this->piecewise_coeffs_[piece][0];
  float n = this->piecewise_coeffs_[piece][1];

  // Evaluate the position from that information
  this->position = m * this->time_position_ + n;
}

}  // namespace time_based
}  // namespace esphome
