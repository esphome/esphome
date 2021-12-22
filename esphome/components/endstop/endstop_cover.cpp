#include "endstop_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace endstop {

static const char *const TAG = "endstop.cover";

using namespace esphome::cover;

CoverTraits EndstopCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  traits.set_is_assumed_state(false);
  return traits;
}
void EndstopCover::control(const CoverCall &call) {
  if (call.get_stop()) {
    this->start_direction_(COVER_OPERATION_IDLE);
    this->publish_state();
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    if (pos == this->position) {
      // already at target
    } else {
      auto op = pos < this->position ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING;
      this->target_position_ = pos;
      this->start_direction_(op);
    }
  }
}
void EndstopCover::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  }

  if (this->is_open_()) {
    this->position = COVER_OPEN;
  } else if (this->is_closed_()) {
    this->position = COVER_CLOSED;
  } else if (!restore.has_value()) {
    this->position = 0.5f;
  }
}
void EndstopCover::loop() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  const uint32_t now = millis();

  if (this->current_operation == COVER_OPERATION_OPENING && this->is_open_()) {
    float dur = (now - this->start_dir_time_) / 1e3f;
    ESP_LOGD(TAG, "'%s' - Open endstop reached. Took %.1fs.", this->name_.c_str(), dur);

    this->start_direction_(COVER_OPERATION_IDLE);
    this->position = COVER_OPEN;
    this->publish_state();
  } else if (this->current_operation == COVER_OPERATION_CLOSING && this->is_closed_()) {
    float dur = (now - this->start_dir_time_) / 1e3f;
    ESP_LOGD(TAG, "'%s' - Close endstop reached. Took %.1fs.", this->name_.c_str(), dur);

    this->start_direction_(COVER_OPERATION_IDLE);
    this->position = COVER_CLOSED;
    this->publish_state();
  } else if (now - this->start_dir_time_ > this->max_duration_) {
    ESP_LOGD(TAG, "'%s' - Max duration reached. Stopping cover.", this->name_.c_str());
    this->start_direction_(COVER_OPERATION_IDLE);
    this->publish_state();
  }

  // Recompute position every loop cycle
  this->recompute_position_();

  if (this->current_operation != COVER_OPERATION_IDLE && this->is_at_target_()) {
    this->start_direction_(COVER_OPERATION_IDLE);
    this->publish_state();
  }

  // Send current position every second
  if (this->current_operation != COVER_OPERATION_IDLE && now - this->last_publish_time_ > 1000) {
    this->publish_state(false);
    this->last_publish_time_ = now;
  }
}
void EndstopCover::dump_config() {
  LOG_COVER("", "Endstop Cover", this);
  LOG_BINARY_SENSOR("  ", "Open Endstop", this->open_endstop_);
  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
  LOG_BINARY_SENSOR("  ", "Close Endstop", this->close_endstop_);
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
}
float EndstopCover::get_setup_priority() const { return setup_priority::DATA; }
void EndstopCover::stop_prev_trigger_() {
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}
bool EndstopCover::is_at_target_() const {
  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      if (this->target_position_ == COVER_OPEN)
        return this->is_open_();
      return this->position >= this->target_position_;
    case COVER_OPERATION_CLOSING:
      if (this->target_position_ == COVER_CLOSED)
        return this->is_closed_();
      return this->position <= this->target_position_;
    case COVER_OPERATION_IDLE:
    default:
      return true;
  }
}
void EndstopCover::start_direction_(CoverOperation dir) {
  if (dir == this->current_operation)
    return;

  this->recompute_position_();
  Trigger<> *trig;
  switch (dir) {
    case COVER_OPERATION_IDLE:
      trig = this->stop_trigger_;
      break;
    case COVER_OPERATION_OPENING:
      trig = this->open_trigger_;
      break;
    case COVER_OPERATION_CLOSING:
      trig = this->close_trigger_;
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
void EndstopCover::recompute_position_() {
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
  this->position += dir * (now - this->last_recompute_time_) / action_dur;
  this->position = clamp(this->position, 0.0f, 1.0f);

  this->last_recompute_time_ = now;
}

}  // namespace endstop
}  // namespace esphome
