#include "sb_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

namespace esphome {
namespace single_button_garage_door {

static const char *const TAG = "singlebutton.cover";

using namespace esphome::cover;

CoverTraits SingleButtonCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(false);
  return traits;
}
void SingleButtonCover::control(const CoverCall &call) {
  ESP_LOGD(TAG, "control start");
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
    } else {
      auto op = pos < this->position ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING;
      this->target_position_ = pos;
      this->start_direction_(op);
    }
  }
}
void SingleButtonCover::setup() {
  ESP_LOGD(TAG, "setup start");
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
    ESP_LOGD(TAG, "setup => restored pos");
  }

  if (this->is_open_()) {
    ESP_LOGD(TAG, "setup => open");
    this->position = COVER_OPEN;
  } else if (this->is_closed_()) {
    this->position = COVER_CLOSED;
    ESP_LOGD(TAG, "setup => closed");
  } else if (!restore.has_value()) {
    this->position = 0.5f;
    ESP_LOGD(TAG, "setup => 0.5");
  }
  this->was_closed_ = this->is_closed_();
  this->was_open_ = this->is_open_();

  ESP_LOGD(TAG, "setup end.");
}
void SingleButtonCover::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // sometime the garage door can be opened or closed using original remote control or manually the motor button
  // in those cases, a "resync" is needed because the start_direction was not called
  // the only way to detect this is through endstops

  if (now - this->last_resync_time_ > 1000) {
    this->last_resync_time_ = now;

    if (this->is_open_() && !(this->position == COVER_OPEN)) {
      ESP_LOGD(TAG, "open endstop => resync pos ");
      this->position = COVER_OPEN;
      this->publish_state();
    } else if (this->is_closed_() && !(this->position == COVER_CLOSED)) {
      ESP_LOGD(TAG, "closed endstop => resync pos ");
      this->position = COVER_CLOSED;
      this->publish_state();
    }
  }

  if (this->is_open_() && !this->was_open_) {
    ESP_LOGD(TAG, "open endstop just reached, may need to resync");
    // this->start_direction_(COVER_OPERATION_IDLE);
    this->position = COVER_OPEN;
    this->publish_state();
  } else if (this->is_closed_() && !this->was_closed_) {
    ESP_LOGD(TAG, "closed endstop just reached, may need to resync");
    // this->start_direction_(COVER_OPERATION_IDLE);
    this->position = COVER_CLOSED;
    this->publish_state();
  } else if (!this->is_closed_() && this->was_closed_ && !(this->current_operation == COVER_OPERATION_OPENING)) {
    ESP_LOGD(TAG, "closed endstop just left, may need to resync");
    // this->start_direction_(COVER_OPERATION_IDLE);
    this->current_operation = COVER_OPERATION_OPENING;
    this->position = COVER_CLOSED;
    this->start_dir_time_ = now;
    this->last_recompute_time_ = now;
    this->publish_state();
  } else if (!this->is_open_() && this->was_open_ && !(this->current_operation == COVER_OPERATION_CLOSING)) {
    ESP_LOGD(TAG, "open endstop just left, may need to resync");
    // this->start_direction_(COVER_OPERATION_IDLE);
    this->current_operation = COVER_OPERATION_CLOSING;
    this->position = COVER_CLOSED;
    this->start_dir_time_ = now;
    this->last_recompute_time_ = now;
    this->publish_state();
  }

  this->was_closed_ = this->is_closed_();
  this->was_open_ = this->is_open_();

  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

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
    ESP_LOGD(TAG, "'%s' - Max duration reached. cover must be idle now.", this->name_.c_str());
    // this->start_direction_(COVER_OPERATION_IDLE);
    this->current_operation = COVER_OPERATION_IDLE;
    this->publish_state();
  }

  // Recompute position every loop cycle
  this->recompute_position_();

  // this code is commented for now because if we use orinal remote control,
  // it immediatly stops the door (because target position is unknown)
  // todo find a solution to "wait for" endstop left before checking this
  // if (this->current_operation != COVER_OPERATION_IDLE && this->is_at_target_()) {
  //   ESP_LOGD(TAG,"stop because pos already at target");
  //   this->start_direction_(COVER_OPERATION_IDLE);
  //   this->publish_state();
  // }

  // Send current position every second
  if (this->current_operation != COVER_OPERATION_IDLE && now - this->last_publish_time_ > 1000) {
    ESP_LOGD(TAG, "closed=%s , position=%f (closed is 0)", this->is_closed_() ? "true" : "false", this->position);
    this->publish_state(false);
    this->last_publish_time_ = now;
  }
}
void SingleButtonCover::dump_config() {
  LOG_COVER("", "Endstop Cover", this);
  LOG_BINARY_SENSOR("  ", "Open Endstop", this->open_endstop_);
  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
  LOG_BINARY_SENSOR("  ", "Close Endstop", this->close_endstop_);
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
}
float SingleButtonCover::get_setup_priority() const { return setup_priority::DATA; }
void SingleButtonCover::stop_prev_trigger_() {
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}
bool SingleButtonCover::is_at_target_() const {
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
void SingleButtonCover::start_direction_(CoverOperation dir) {
  if (dir == this->current_operation)
    return;

  this->recompute_position_();
  Trigger<> *trig;
  trig = this->single_button_trigger_;
  int clic_count_needed = 0;

  switch (dir) {
    case COVER_OPERATION_IDLE:
      clic_count_needed = 1;
      break;
    case COVER_OPERATION_OPENING:

      switch (this->current_operation) {
        case COVER_OPERATION_IDLE:
          clic_count_needed = 1;
          break;
        case COVER_OPERATION_OPENING:
          // shoud not occur
          break;
        case COVER_OPERATION_CLOSING:
          clic_count_needed = 3;
          break;
      }
      this->last_operation_ = dir;

      break;
    case COVER_OPERATION_CLOSING:

      switch (this->current_operation) {
        case COVER_OPERATION_IDLE:
          clic_count_needed = 1;
          break;
        case COVER_OPERATION_OPENING:
          clic_count_needed = 3;
          break;
        case COVER_OPERATION_CLOSING:
          // shoud not occur
          break;
      }

      this->last_operation_ = dir;

      break;
    default:
      return;
  }

  this->current_operation = dir;

  // this->stop_prev_trigger_();
  int i = 0;
  for (i = 0; i < clic_count_needed; i++) {
    trig->trigger();
    delay(500);
  }
  this->prev_command_trigger_ = trig;

  const uint32_t now = millis();
  this->start_dir_time_ = now;
  this->last_recompute_time_ = now;
}
void SingleButtonCover::recompute_position_() {
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

}  // namespace single_button_garage_door
}  // namespace esphome
