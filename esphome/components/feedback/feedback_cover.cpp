#include "feedback_cover.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::feedback {

ESPHOME_LOG_TAG(TAG, "feedback.cover");

static constexpr uint32_t DIRECTION_CHANGE_TIMEOUT_ID = 1;

using namespace esphome::cover;

void FeedbackCover::setup() {
  auto restore = this->restore_state_();

  if (restore.has_value()) {
    restore->apply(this);
  } else {
    // if no other information, assume half open
    this->position = 0.5f;
  }
  this->current_operation = COVER_OPERATION_IDLE;

#ifdef USE_FEEDBACK_COVER_ENDSTOP
  if (this->open_endstop_ != nullptr && this->open_endstop_->state) {
    this->position = COVER_OPEN;
  } else if (this->close_endstop_ != nullptr && this->close_endstop_->state) {
    this->position = COVER_CLOSED;
  }
#endif

#ifdef USE_FEEDBACK_COVER_MOVEMENT
  if (this->open_feedback_ != nullptr && this->open_feedback_->state) {
    this->current_operation = COVER_OPERATION_OPENING;
  } else if (this->close_feedback_ != nullptr && this->close_feedback_->state) {
    this->current_operation = COVER_OPERATION_CLOSING;
  }
#endif

  this->last_recompute_time_ = this->start_dir_time_ = App.get_loop_component_start_time();
}

CoverTraits FeedbackCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(this->assumed_state_);
  return traits;
}

void FeedbackCover::dump_config() {
  LOG_COVER("", "Feedback Cover", this);
  ESP_LOGCONFIG(TAG,
                "  Open Duration: %.1fs\n"
                "  Close Duration: %.1fs",
                this->open_duration_ / 1e3f, this->close_duration_ / 1e3f);
#ifdef USE_FEEDBACK_COVER_ENDSTOP
  LOG_BINARY_SENSOR("  ", "Open Endstop", this->open_endstop_);
  LOG_BINARY_SENSOR("  ", "Close Endstop", this->close_endstop_);
#endif
#ifdef USE_FEEDBACK_COVER_MOVEMENT
  LOG_BINARY_SENSOR("  ", "Open Feedback", this->open_feedback_);
  LOG_BINARY_SENSOR("  ", "Close Feedback", this->close_feedback_);
#endif
#ifdef USE_FEEDBACK_COVER_OBSTACLE
  LOG_BINARY_SENSOR("  ", "Open Obstacle", this->open_obstacle_);
  LOG_BINARY_SENSOR("  ", "Close Obstacle", this->close_obstacle_);
#endif
  if (this->has_built_in_endstop_) {
    ESP_LOGCONFIG(TAG, "  Has builtin endstop: YES");
  }
#ifdef USE_FEEDBACK_COVER_MOVEMENT
  if (this->infer_endstop_) {
    ESP_LOGCONFIG(TAG, "  Infer endstop from movement: YES");
  }
#endif
  if (this->max_duration_ < UINT32_MAX) {
    ESP_LOGCONFIG(TAG, "  Max Duration: %.1fs", this->max_duration_ / 1e3f);
  }
  if (this->direction_change_waittime_ != UINT32_MAX) {
    ESP_LOGCONFIG(TAG, "  Direction change wait time: %.1fs", this->direction_change_waittime_ / 1e3f);
  }
  if (this->acceleration_wait_time_) {
    ESP_LOGCONFIG(TAG, "  Acceleration wait time: %.1fs", this->acceleration_wait_time_ / 1e3f);
  }
#ifdef USE_FEEDBACK_COVER_OVERSHOOT
  if (this->overshoot_duration_) {
    ESP_LOGCONFIG(TAG, "  Overshoot duration: %.1fs", this->overshoot_duration_ / 1e3f);
  }
#endif
#ifdef USE_FEEDBACK_COVER_OBSTACLE
  if (this->obstacle_rollback_) {
    ESP_LOGCONFIG(TAG, "  Obstacle rollback: %.1f%%", this->obstacle_rollback_ * 100);
  }
#endif
}

#ifdef USE_FEEDBACK_COVER_MOVEMENT
void FeedbackCover::set_open_sensor(binary_sensor::BinarySensor *open_feedback) {
  this->open_feedback_ = open_feedback;
  open_feedback->add_on_state_callback([this](bool state) { this->on_feedback_(true, state); });
}

void FeedbackCover::set_close_sensor(binary_sensor::BinarySensor *close_feedback) {
  this->close_feedback_ = close_feedback;
  close_feedback->add_on_state_callback([this](bool state) { this->on_feedback_(false, state); });
}

void FeedbackCover::on_feedback_(bool open, bool state) {
  const CoverOperation operation = open ? COVER_OPERATION_OPENING : COVER_OPERATION_CLOSING;
  ESP_LOGD(TAG, "'%s' - %s feedback '%s'.", LOG_STR_ARG(this->get_log_name()),
           open ? LOG_STR_LITERAL("Open") : LOG_STR_LITERAL("Close"),
           state ? LOG_STR_LITERAL("STARTED") : LOG_STR_LITERAL("ENDED"));
  this->recompute_position_();
  if (!state && this->infer_endstop_ && this->current_trigger_operation_ == operation) {
    this->endstop_reached_(open);
  }
  this->set_current_operation_(state ? operation : COVER_OPERATION_IDLE, false);
}
#endif

#ifdef USE_FEEDBACK_COVER_ENDSTOP
void FeedbackCover::set_open_endstop(binary_sensor::BinarySensor *open_endstop) {
  this->open_endstop_ = open_endstop;
  open_endstop->add_on_state_callback([this](bool state) {
    if (state)
      this->endstop_reached_(true);
  });
}

void FeedbackCover::set_close_endstop(binary_sensor::BinarySensor *close_endstop) {
  this->close_endstop_ = close_endstop;
  close_endstop->add_on_state_callback([this](bool state) {
    if (state)
      this->endstop_reached_(false);
  });
}
#endif

void FeedbackCover::endstop_reached_(bool open_endstop) {
  const uint32_t now = App.get_loop_component_start_time();

  this->position = open_endstop ? COVER_OPEN : COVER_CLOSED;

  // Only when moving towards it; the cover may be returning from just past it
  if (this->current_trigger_operation_ == (open_endstop ? COVER_OPERATION_OPENING : COVER_OPERATION_CLOSING)) {
    float dur = (now - this->start_dir_time_) / 1e3f;
    ESP_LOGD(TAG, "'%s' - %s endstop reached. Took %.1fs.", LOG_STR_ARG(this->get_log_name()),
             open_endstop ? LOG_STR_LITERAL("Open") : LOG_STR_LITERAL("Close"), dur);

    if (!this->has_built_in_endstop_) {
      this->start_direction_(COVER_OPERATION_IDLE);
    } else {
      this->set_current_operation_(COVER_OPERATION_IDLE, true);
    }
    // Without movement sensors the stop above already published
    if (!this->has_movement_sensors_())
      return;
  }

  this->publish_state();
  this->last_publish_time_ = now;
}

void FeedbackCover::set_current_operation_(cover::CoverOperation operation, bool is_triggered) {
  if (is_triggered) {
    this->current_trigger_operation_ = operation;
    // With movement sensors they set the actual operation
    if (this->has_movement_sensors_())
      return;
  }

  const uint32_t now = App.get_loop_component_start_time();
  this->current_operation = operation;
  if (operation != COVER_OPERATION_IDLE)
    this->enable_loop();
  this->start_dir_time_ = this->last_recompute_time_ = now;
  this->publish_state();
  this->last_publish_time_ = now;
}

#ifdef USE_FEEDBACK_COVER_OBSTACLE
void FeedbackCover::set_open_obstacle_sensor(binary_sensor::BinarySensor *open_obstacle) {
  this->open_obstacle_ = open_obstacle;
  open_obstacle->add_on_state_callback([this](bool state) { this->on_obstacle_(true, state); });
}

void FeedbackCover::set_close_obstacle_sensor(binary_sensor::BinarySensor *close_obstacle) {
  this->close_obstacle_ = close_obstacle;
  close_obstacle->add_on_state_callback([this](bool state) { this->on_obstacle_(false, state); });
}

void FeedbackCover::on_obstacle_(bool open, bool state) {
  const CoverOperation operation = open ? COVER_OPERATION_OPENING : COVER_OPERATION_CLOSING;
  if (!state || (this->current_operation != operation && this->current_trigger_operation_ != operation))
    return;
  ESP_LOGD(TAG, "'%s' - %s obstacle detected.", LOG_STR_ARG(this->get_log_name()),
           open ? LOG_STR_LITERAL("Open") : LOG_STR_LITERAL("Close"));
  this->start_direction_(COVER_OPERATION_IDLE);

  if (this->obstacle_rollback_) {
    const float rollback = open ? -this->obstacle_rollback_ : this->obstacle_rollback_;
    this->target_position_ = clamp(this->position + rollback, COVER_CLOSED, COVER_OPEN);
    this->start_direction_(open ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING);
  }
}
#endif

void FeedbackCover::loop() {
  if (this->current_operation == COVER_OPERATION_IDLE) {
    this->disable_loop();
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();

  this->recompute_position_();

  // if we initiated the move, check if we reached position or max time
  // (stopping from endstop sensor is handled in callback)
  if (this->current_trigger_operation_ != COVER_OPERATION_IDLE) {
    if (this->is_at_target_()) {
      const bool at_end = this->target_position_ == COVER_OPEN || this->target_position_ == COVER_CLOSED;
      if (this->has_built_in_endstop_ && at_end) {
        // Don't trigger stop, let the cover stop by itself.
        this->set_current_operation_(COVER_OPERATION_IDLE, true);
#ifdef USE_FEEDBACK_COVER_OVERSHOOT
      } else if (at_end && this->overshoot_duration_) {
        // The define is global, so a cover without the option still lands here with a zero duration
        // Keep driving past the end position for a while so accumulated timing error is squeezed out
        if (!this->start_overshoot_time_) {
          this->start_overshoot_time_ = now;
        } else if (now - this->start_overshoot_time_ > this->overshoot_duration_ ||
                   now - this->start_dir_time_ > this->max_duration_) {
          ESP_LOGD(TAG, "'%s' - Overshoot duration reached. Stopping cover.", LOG_STR_ARG(this->get_log_name()));
          this->start_direction_(COVER_OPERATION_IDLE);
        }
#endif
      } else {
        this->start_direction_(COVER_OPERATION_IDLE);
      }
    } else if (now - this->start_dir_time_ > this->max_duration_) {
      ESP_LOGD(TAG, "'%s' - Max duration reached. Stopping cover.", LOG_STR_ARG(this->get_log_name()));
      this->start_direction_(COVER_OPERATION_IDLE);
    }
  }

  // Publish intermediate positions without saving them
  if (now - this->last_publish_time_ > this->update_interval_) {
    this->publish_state(false);
    this->last_publish_time_ = now;
  }
}

void FeedbackCover::control(const CoverCall &call) {
  if (call.get_stop()) {
    this->start_direction_(COVER_OPERATION_IDLE);
  } else if (call.get_toggle().has_value()) {
    if (this->current_trigger_operation_ != COVER_OPERATION_IDLE) {
      this->start_direction_(COVER_OPERATION_IDLE);
    } else {
      if (this->position == COVER_CLOSED || this->last_operation_ == COVER_OPERATION_CLOSING) {
        this->target_position_ = COVER_OPEN;
        this->start_direction_(COVER_OPERATION_OPENING);
      } else {
        this->target_position_ = COVER_CLOSED;
        this->start_direction_(COVER_OPERATION_CLOSING);
      }
    }
  } else {
    auto pos_opt = call.get_position();
    if (!pos_opt.has_value())
      return;
    auto pos = *pos_opt;
    if (pos == this->position) {
      // Resend in case the assumed state is wrong
      if (this->has_built_in_endstop_ &&
          ((pos == COVER_OPEN && !this->has_endstop_(true)) || (pos == COVER_CLOSED && !this->has_endstop_(false)))) {
        this->target_position_ = pos;
        this->start_direction_(pos == COVER_CLOSED ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING);
      } else if (this->current_operation != COVER_OPERATION_IDLE ||
                 this->current_trigger_operation_ != COVER_OPERATION_IDLE) {
        this->start_direction_(COVER_OPERATION_IDLE);
      }
    } else {
      this->target_position_ = pos;
      this->start_direction_(pos < this->position ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING);
    }
  }
}

void FeedbackCover::stop_prev_trigger_() {
  if (this->direction_change_waittime_ != UINT32_MAX) {
    this->cancel_timeout(DIRECTION_CHANGE_TIMEOUT_ID);
  }
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}

bool FeedbackCover::is_at_target_() const {
  switch (this->current_trigger_operation_) {
    case COVER_OPERATION_OPENING:
      return this->position >= this->target_position_;
    case COVER_OPERATION_CLOSING:
      return this->position <= this->target_position_;
    case COVER_OPERATION_IDLE:
      return this->current_operation == COVER_OPERATION_IDLE;
    default:
      return true;
  }
}

void FeedbackCover::start_direction_(CoverOperation dir) {
  Trigger<> *trig;
#ifdef USE_FEEDBACK_COVER_OBSTACLE
  binary_sensor::BinarySensor *obstacle{nullptr};
#endif

#ifdef USE_FEEDBACK_COVER_OVERSHOOT
  // Any new command ends a running overshoot
  this->start_overshoot_time_ = 0;
#endif

  switch (dir) {
    case COVER_OPERATION_IDLE:
      trig = &this->stop_trigger_;
      break;
    case COVER_OPERATION_OPENING:
      this->last_operation_ = dir;
      trig = &this->open_trigger_;
#ifdef USE_FEEDBACK_COVER_OBSTACLE
      obstacle = this->open_obstacle_;
#endif
      break;
    case COVER_OPERATION_CLOSING:
      this->last_operation_ = dir;
      trig = &this->close_trigger_;
#ifdef USE_FEEDBACK_COVER_OBSTACLE
      obstacle = this->close_obstacle_;
#endif
      break;
    default:
      return;
  }

  this->stop_prev_trigger_();

#ifdef USE_FEEDBACK_COVER_OBSTACLE
  // An obstacle already present aborts the start
  if (obstacle != nullptr && obstacle->state) {
    ESP_LOGD(TAG, "'%s' - %s obstacle detected. Action not started.", LOG_STR_ARG(this->get_log_name()),
             dir == COVER_OPERATION_OPENING ? LOG_STR_LITERAL("Open") : LOG_STR_LITERAL("Close"));
    return;
  }
#endif

  if (this->direction_change_waittime_ != UINT32_MAX && dir != COVER_OPERATION_IDLE &&
      this->current_operation != COVER_OPERATION_IDLE && dir != this->current_operation) {
    ESP_LOGD(TAG, "'%s' - Reversing direction.", LOG_STR_ARG(this->get_log_name()));
    this->start_direction_(COVER_OPERATION_IDLE);
    this->set_timeout(DIRECTION_CHANGE_TIMEOUT_ID, this->direction_change_waittime_,
                      [this, dir]() { this->start_direction_(dir); });
  } else {
    this->set_current_operation_(dir, true);
    this->prev_command_trigger_ = trig;
    ESP_LOGD(TAG, "'%s' - Firing '%s' trigger.", LOG_STR_ARG(this->get_log_name()),
             dir == COVER_OPERATION_OPENING   ? LOG_STR_LITERAL("OPEN")
             : dir == COVER_OPERATION_CLOSING ? LOG_STR_LITERAL("CLOSE")
                                              : LOG_STR_LITERAL("STOP"));
    trig->trigger();
  }
}

void FeedbackCover::recompute_position_() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  const uint32_t now = App.get_loop_component_start_time();
  float dir;
  float action_dur;
  float min_pos;
  float max_pos;

  // With an endstop the estimate stops short of the end; the endstop callback sets it
  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      dir = 1.0f;
      action_dur = this->open_duration_;
      min_pos = COVER_CLOSED;
      max_pos = this->has_endstop_(true) && this->position < COVER_OPEN ? 0.99f : COVER_OPEN;
      break;
    case COVER_OPERATION_CLOSING:
      dir = -1.0f;
      action_dur = this->close_duration_;
      min_pos = this->has_endstop_(false) && this->position > COVER_CLOSED ? 0.01f : COVER_CLOSED;
      max_pos = COVER_OPEN;
      break;
    default:
      return;
  }

  if (now - this->start_dir_time_ > this->acceleration_wait_time_) {
    uint32_t accel_end_time = this->start_dir_time_ + this->acceleration_wait_time_;
    uint32_t effective_start;
    if (static_cast<int32_t>(accel_end_time - this->last_recompute_time_) >= 0) {
      effective_start = accel_end_time;
    } else {
      effective_start = this->last_recompute_time_;
    }
    this->position += dir * (now - effective_start) / (action_dur - this->acceleration_wait_time_);
    this->position = clamp(this->position, min_pos, max_pos);
  }
  this->last_recompute_time_ = now;
}

}  // namespace esphome::feedback
