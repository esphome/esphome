#include "feedback_cover.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace feedback {

static const char *const TAG = "feedback.cover";

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

#ifdef USE_BINARY_SENSOR
  // if available, get position from endstop sensors
  if (this->open_endstop_ != nullptr && this->open_endstop_->state) {
    this->position = COVER_OPEN;
  } else if (this->close_endstop_ != nullptr && this->close_endstop_->state) {
    this->position = COVER_CLOSED;
  }

  // if available, get moving state from sensors
  if (this->open_feedback_ != nullptr && this->open_feedback_->state) {
    this->current_operation = COVER_OPERATION_OPENING;
  } else if (this->close_feedback_ != nullptr && this->close_feedback_->state) {
    this->current_operation = COVER_OPERATION_CLOSING;
  }
#endif

  this->last_recompute_time_ = this->start_dir_time_ = millis();
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
  LOG_COVER("", "Endstop Cover", this);
  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Open Endstop", this->open_endstop_);
  LOG_BINARY_SENSOR("  ", "Open Feedback", this->open_feedback_);
  LOG_BINARY_SENSOR("  ", "Open Obstacle", this->open_obstacle_);
#endif
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Close Endstop", this->close_endstop_);
  LOG_BINARY_SENSOR("  ", "Close Feedback", this->close_feedback_);
  LOG_BINARY_SENSOR("  ", "Close Obstacle", this->close_obstacle_);
#endif
  if (this->has_built_in_endstop_) {
    ESP_LOGCONFIG(TAG, "  Has builtin endstop: YES");
  }
  if (this->infer_endstop_) {
    ESP_LOGCONFIG(TAG, "  Infer endstop from movement: YES");
  }
  if (this->max_duration_ < UINT32_MAX) {
    ESP_LOGCONFIG(TAG, "  Max Duration: %.1fs", this->max_duration_ / 1e3f);
  }
  if (this->direction_change_waittime_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Direction change wait time: %.1fs", *this->direction_change_waittime_ / 1e3f);
  }
  if (this->acceleration_wait_time_) {
    ESP_LOGCONFIG(TAG, "  Acceleration wait time: %.1fs", this->acceleration_wait_time_ / 1e3f);
  }
#ifdef USE_BINARY_SENSOR
  if (this->obstacle_rollback_ && (this->open_obstacle_ != nullptr || this->close_obstacle_ != nullptr)) {
    ESP_LOGCONFIG(TAG, "  Obstacle rollback: %.1f%%", this->obstacle_rollback_ * 100);
  }
#endif
}

#ifdef USE_BINARY_SENSOR

void FeedbackCover::set_open_sensor(binary_sensor::BinarySensor *open_feedback) {
  this->open_feedback_ = open_feedback;

  // setup callbacks to react to sensor changes
  open_feedback->add_on_state_callback([this](bool state) {
    ESP_LOGD(TAG, "'%s' - Open feedback '%s'.", this->name_.c_str(), state ? "STARTED" : "ENDED");
    this->recompute_position_();
    if (!state && this->infer_endstop_ && this->current_trigger_operation_ == COVER_OPERATION_OPENING) {
      this->endstop_reached_(true);
    }
    this->set_current_operation_(state ? COVER_OPERATION_OPENING : COVER_OPERATION_IDLE, false);
  });
}

void FeedbackCover::set_close_sensor(binary_sensor::BinarySensor *close_feedback) {
  this->close_feedback_ = close_feedback;

  close_feedback->add_on_state_callback([this](bool state) {
    ESP_LOGD(TAG, "'%s' - Close feedback '%s'.", this->name_.c_str(), state ? "STARTED" : "ENDED");
    this->recompute_position_();
    if (!state && this->infer_endstop_ && this->current_trigger_operation_ == COVER_OPERATION_CLOSING) {
      this->endstop_reached_(false);
    }

    this->set_current_operation_(state ? COVER_OPERATION_CLOSING : COVER_OPERATION_IDLE, false);
  });
}

void FeedbackCover::set_open_endstop(binary_sensor::BinarySensor *open_endstop) {
  this->open_endstop_ = open_endstop;
  open_endstop->add_on_state_callback([this](bool state) {
    if (state) {
      this->endstop_reached_(true);
    }
  });
}

void FeedbackCover::set_close_endstop(binary_sensor::BinarySensor *close_endstop) {
  this->close_endstop_ = close_endstop;
  close_endstop->add_on_state_callback([this](bool state) {
    if (state) {
      this->endstop_reached_(false);
    }
  });
}
#endif

void FeedbackCover::endstop_reached_(bool open_endstop) {
  const uint32_t now = millis();

  this->position = open_endstop ? COVER_OPEN : COVER_CLOSED;

  // only act if endstop activated while moving in the right direction, in case we are coming back
  // from a position slightly past the endpoint
  if (this->current_trigger_operation_ == (open_endstop ? COVER_OPERATION_OPENING : COVER_OPERATION_CLOSING)) {
    float dur = (now - this->start_dir_time_) / 1e3f;
    ESP_LOGD(TAG, "'%s' - %s endstop reached. Took %.1fs.", this->name_.c_str(), open_endstop ? "Open" : "Close", dur);

    // if there is no external mechanism, stop the cover
    if (!this->has_built_in_endstop_) {
      this->start_direction_(COVER_OPERATION_IDLE);
    } else {
      this->set_current_operation_(COVER_OPERATION_IDLE, true);
    }
  }

  // always sync position and publish
  this->publish_state();
  this->last_publish_time_ = now;
}

void FeedbackCover::set_current_operation_(cover::CoverOperation operation, bool is_triggered) {
  if (is_triggered) {
    this->current_trigger_operation_ = operation;
  }

  // if it is setting the actual operation (not triggered one) or
  // if we don't have moving sensor, we operate in optimistic mode, assuming actions take place immediately
  // thus, triggered operation always sets current operation.
  // otherwise, current operation comes from sensor, and may differ from requested operation
  // this might be from delays or complex actions, or because the movement was not trigger by the component
  // but initiated externally

#ifdef USE_BINARY_SENSOR
  if (!is_triggered || (this->open_feedback_ == nullptr || this->close_feedback_ == nullptr))
#endif
  {
    auto now = millis();
    this->current_operation = operation;
    this->start_dir_time_ = this->last_recompute_time_ = now;
    this->publish_state();
    this->last_publish_time_ = now;
  }
}

#ifdef USE_BINARY_SENSOR
void FeedbackCover::set_close_obstacle_sensor(binary_sensor::BinarySensor *close_obstacle) {
  this->close_obstacle_ = close_obstacle;

  close_obstacle->add_on_state_callback([this](bool state) {
    if (state && (this->current_operation == COVER_OPERATION_CLOSING ||
                  this->current_trigger_operation_ == COVER_OPERATION_CLOSING)) {
      ESP_LOGD(TAG, "'%s' - Close obstacle detected.", this->name_.c_str());
      this->start_direction_(COVER_OPERATION_IDLE);

      if (this->obstacle_rollback_) {
        this->target_position_ = clamp(this->position + this->obstacle_rollback_, COVER_CLOSED, COVER_OPEN);
        this->start_direction_(COVER_OPERATION_OPENING);
      }
    }
  });
}

void FeedbackCover::set_open_obstacle_sensor(binary_sensor::BinarySensor *open_obstacle) {
  this->open_obstacle_ = open_obstacle;

  open_obstacle->add_on_state_callback([this](bool state) {
    if (state && (this->current_operation == COVER_OPERATION_OPENING ||
                  this->current_trigger_operation_ == COVER_OPERATION_OPENING)) {
      ESP_LOGD(TAG, "'%s' - Open obstacle detected.", this->name_.c_str());
      this->start_direction_(COVER_OPERATION_IDLE);

      if (this->obstacle_rollback_) {
        this->target_position_ = clamp(this->position - this->obstacle_rollback_, COVER_CLOSED, COVER_OPEN);
        this->start_direction_(COVER_OPERATION_CLOSING);
      }
    }
  });
}
#endif

void FeedbackCover::loop() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;
  const uint32_t now = millis();

  // Recompute position every loop cycle
  this->recompute_position_();

  // if we initiated the move, check if we reached position or max time
  // (stoping from endstop sensor is handled in callback)
  if (this->current_trigger_operation_ != COVER_OPERATION_IDLE) {
    if (this->is_at_target_()) {
      if (this->has_built_in_endstop_ &&
          (this->target_position_ == COVER_OPEN || this->target_position_ == COVER_CLOSED)) {
        // Don't trigger stop, let the cover stop by itself.
        this->set_current_operation_(COVER_OPERATION_IDLE, true);
      } else {
        this->start_direction_(COVER_OPERATION_IDLE);
      }
    } else if (now - this->start_dir_time_ > this->max_duration_) {
      ESP_LOGD(TAG, "'%s' - Max duration reached. Stopping cover.", this->name_.c_str());
      this->start_direction_(COVER_OPERATION_IDLE);
    }
  }

  // update current position at requested interval, regardless of who started the movement
  // so that we also update UI if there was an external movement
  // don´t save intermediate positions
  if (now - this->last_publish_time_ > this->update_interval_) {
    this->publish_state(false);
    this->last_publish_time_ = now;
  }
}

void FeedbackCover::control(const CoverCall &call) {
  // stop action logic
  if (call.get_stop()) {
    this->start_direction_(COVER_OPERATION_IDLE);
  } else if (call.get_toggle().has_value()) {
    // toggle action logic: OPEN - STOP - CLOSE
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
  } else if (call.get_position().has_value()) {
    // go to position action
    auto pos = *call.get_position();
    if (pos == this->position) {
      // already at target,

      // for covers with built in end stop, if we don´t have sensors we should send the command again
      // to make sure the assumed state is not wrong
      if (this->has_built_in_endstop_ && ((pos == COVER_OPEN
#ifdef USE_BINARY_SENSOR
                                           && this->open_endstop_ == nullptr
#endif
                                           && !this->infer_endstop_) ||
                                          (pos == COVER_CLOSED
#ifdef USE_BINARY_SENSOR
                                           && this->close_endstop_ == nullptr
#endif
                                           && !this->infer_endstop_))) {
        this->target_position_ = pos;
        this->start_direction_(pos == COVER_CLOSED ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING);
      } else if (this->current_operation != COVER_OPERATION_IDLE ||
                 this->current_trigger_operation_ != COVER_OPERATION_IDLE) {
        // if we are moving, stop
        this->start_direction_(COVER_OPERATION_IDLE);
      }
    } else {
      this->target_position_ = pos;
      this->start_direction_(pos < this->position ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING);
    }
  }
}

void FeedbackCover::stop_prev_trigger_() {
  if (this->direction_change_waittime_.has_value()) {
    this->cancel_timeout("direction_change");
  }
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}

bool FeedbackCover::is_at_target_() const {
  // if initiated externally, current operation might be different from
  // operation that was triggered, thus evaluate position against what was asked

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

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *obstacle{nullptr};
#endif

  switch (dir) {
    case COVER_OPERATION_IDLE:
      trig = this->stop_trigger_;
      break;
    case COVER_OPERATION_OPENING:
      this->last_operation_ = dir;
      trig = this->open_trigger_;
#ifdef USE_BINARY_SENSOR
      obstacle = this->open_obstacle_;
#endif
      break;
    case COVER_OPERATION_CLOSING:
      this->last_operation_ = dir;
      trig = this->close_trigger_;
#ifdef USE_BINARY_SENSOR
      obstacle = this->close_obstacle_;
#endif
      break;
    default:
      return;
  }

  this->stop_prev_trigger_();

#ifdef USE_BINARY_SENSOR
  // check if there is an obstacle to start the new operation -> abort without any change
  // the case when an obstacle appears while moving is handled in the callback
  if (obstacle != nullptr && obstacle->state) {
    ESP_LOGD(TAG, "'%s' - %s obstacle detected. Action not started.", this->name_.c_str(),
             dir == COVER_OPERATION_OPENING ? "Open" : "Close");
    return;
  }
#endif

  // if we are moving and need to move in the opposite direction
  // check if we have a wait time
  if (this->direction_change_waittime_.has_value() && dir != COVER_OPERATION_IDLE &&
      this->current_operation != COVER_OPERATION_IDLE && dir != this->current_operation) {
    ESP_LOGD(TAG, "'%s' - Reversing direction.", this->name_.c_str());
    this->start_direction_(COVER_OPERATION_IDLE);

    this->set_timeout("direction_change", *this->direction_change_waittime_,
                      [this, dir]() { this->start_direction_(dir); });

  } else {
    this->set_current_operation_(dir, true);
    this->prev_command_trigger_ = trig;
    ESP_LOGD(TAG, "'%s' - Firing '%s' trigger.", this->name_.c_str(),
             dir == COVER_OPERATION_OPENING   ? "OPEN"
             : dir == COVER_OPERATION_CLOSING ? "CLOSE"
                                              : "STOP");
    trig->trigger();
  }
}

void FeedbackCover::recompute_position_() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  const uint32_t now = millis();
  float dir;
  float action_dur;
  float min_pos;
  float max_pos;

  // endstop sensors update position from their callbacks, and sets the fully open/close value
  // If we have endstop, estimation never reaches the fully open/closed state.
  // but if movement continues past corresponding endstop (inertia), keep the fully open/close state

  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      dir = 1.0f;
      action_dur = this->open_duration_;
      min_pos = COVER_CLOSED;
      max_pos = (
#ifdef USE_BINARY_SENSOR
                    this->open_endstop_ != nullptr ||
#endif
                    this->infer_endstop_) &&
                        this->position < COVER_OPEN
                    ? 0.99f
                    : COVER_OPEN;
      break;
    case COVER_OPERATION_CLOSING:
      dir = -1.0f;
      action_dur = this->close_duration_;
      min_pos = (
#ifdef USE_BINARY_SENSOR
                    this->close_endstop_ != nullptr ||
#endif
                    this->infer_endstop_) &&
                        this->position > COVER_CLOSED
                    ? 0.01f
                    : COVER_CLOSED;
      max_pos = COVER_OPEN;
      break;
    default:
      return;
  }

  // check if we have an acceleration_wait_time, and remove from position computation
  if (now > (this->start_dir_time_ + this->acceleration_wait_time_)) {
    this->position +=
        dir * (now - std::max(this->start_dir_time_ + this->acceleration_wait_time_, this->last_recompute_time_)) /
        (action_dur - this->acceleration_wait_time_);
    this->position = clamp(this->position, min_pos, max_pos);
  }
  this->last_recompute_time_ = now;
}

}  // namespace feedback
}  // namespace esphome
