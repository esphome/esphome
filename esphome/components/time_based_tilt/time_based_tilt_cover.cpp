#include "time_based_tilt_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::time_based_tilt {

static const char *const TAG = "time_based_tilt.cover";

const float TimeBasedTiltCover::TARGET_NONE = -1;

using namespace esphome::cover;

void TimeBasedTiltCover::dump_config() {
  LOG_COVER("", "Time Based Tilt Cover", this);
  ESP_LOGCONFIG(TAG, "  Open Duration: %.3fs", this->open_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Close Duration: %.3fs", this->close_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Tilt Close Duration: %.3fs", this->tilt_close_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Tilt Open Duration: %.3fs", this->tilt_open_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Interlock wait time: %.3fs", this->interlock_wait_time_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Inertia close time: %.3fs", this->inertia_close_time_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Inertia open time: %.3fs", this->inertia_open_time_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Recalibration close time: %.3fs", this->recalibration_close_time_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Recalibration open time: %.3fs", this->recalibration_open_time_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Actuator activation close time: %.3fs", this->actuator_activation_close_time_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Actuator activation open time: %.3fs", this->actuator_activation_open_time_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Close sets tilt: %s", this->close_sets_tilt_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Open sets tilt: %s", this->open_sets_tilt_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Current position: %.4f", this->position);
  ESP_LOGCONFIG(TAG, "  Current tilt: %.4f", this->tilt);
}
void TimeBasedTiltCover::setup() {
  if (this->tilt_close_duration_ == 0 || this->tilt_open_duration_ == 0) {
    this->tilt_close_duration_ = 0;
    this->tilt_open_duration_ = 0;
  }
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->position = 0.5f;
    this->tilt = 0.5f;
  }
}
bool TimeBasedTiltCover::is_at_target_position_() const {
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

bool TimeBasedTiltCover::is_at_target_tilt_() const {
  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      return this->tilt >= this->target_tilt_;
    case COVER_OPERATION_CLOSING:
      return this->tilt <= this->target_tilt_;
    case COVER_OPERATION_IDLE:
    default:
      return true;
  }
}

bool TimeBasedTiltCover::is_at_extreme_position_() const {
  return (this->position == COVER_CLOSED && (tilt_close_duration_ == 0 || this->tilt == COVER_CLOSED)) ||
         (this->position == COVER_OPEN && (tilt_open_duration_ == 0 || this->tilt == COVER_OPEN));
}

void TimeBasedTiltCover::loop() {
  if (this->fsm_state_ == STATE_IDLE && this->target_position_ == TARGET_NONE && this->target_tilt_ == TARGET_NONE)
    return;

  const uint32_t now = millis();

  // recalibrating in extreme postions
  if (this->fsm_state_ == STATE_CALIBRATING) {
    if (now - this->last_recompute_time_ >= this->current_recalibration_time_) {
      this->fsm_state_ = STATE_STOPPING;
      this->target_position_ = TARGET_NONE;
      this->target_tilt_ = TARGET_NONE;
      ESP_LOGD(TAG, "Transition to the stopping state");
    }
    return;
  }

  // STOPPING – determining the direction of the last movement and the stopping time. Necessary to support interlocking.
  if (this->fsm_state_ == STATE_STOPPING) {
    this->stop_trigger_->trigger();
    if (this->current_operation != COVER_OPERATION_IDLE) {
      this->interlocked_time_ = millis();
      this->interlocked_direction_ =
          this->current_operation == COVER_OPERATION_CLOSING ? COVER_OPERATION_OPENING : COVER_OPERATION_CLOSING;
    } else {
      this->interlocked_direction_ = COVER_OPERATION_IDLE;
    }
    this->fsm_state_ = STATE_IDLE;
    ESP_LOGD(TAG, "Transition to the idle state");
    this->last_operation_ = this->current_operation;
    this->current_operation = COVER_OPERATION_IDLE;
    this->position = this->round_position(this->position);
    this->tilt = this->round_position(this->tilt);
    this->publish_state();
    return;
  }

  // If the cover is not moving, check whether new targets are set. If they are, compute the movement direction.
  if (this->fsm_state_ == STATE_IDLE && (this->target_position_ != TARGET_NONE || this->target_tilt_ != TARGET_NONE)) {
    if (this->target_position_ != TARGET_NONE) {  // First, calculate based on the target position.
      this->current_operation = this->compute_direction(this->target_position_, this->position);
      if (this->current_operation == COVER_OPERATION_IDLE) {  // Already at the target position.
        this->target_position_ = TARGET_NONE;
        if (this->target_tilt_ != TARGET_NONE) {  // Calculate the direction based on the target tilt.
          this->current_operation = this->compute_direction(this->target_tilt_, this->tilt);
        }
      }
    } else {  // Calculate the direction based on the target tilt.
      this->current_operation = this->compute_direction(this->target_tilt_, this->tilt);
    }

    if (this->current_operation == COVER_OPERATION_IDLE) {  // Already at the target tilt and target position.
      if (this->is_at_extreme_position_()) {  // But if we are at extreme position we can run recacalibration
        this->current_operation = this->position == COVER_CLOSED ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING;
        this->target_position_ = this->position == COVER_CLOSED ? COVER_CLOSED : COVER_OPEN;
      } else {
        this->target_tilt_ = TARGET_NONE;
        this->target_position_ = TARGET_NONE;
        return;
      }
    }

    // Interlocking support.
    if (this->current_operation == this->interlocked_direction_ &&
        now - this->interlocked_time_ < this->interlock_wait_time_)
      return;

    Trigger<> *trig = this->current_operation == COVER_OPERATION_CLOSING ? this->close_trigger_ : this->open_trigger_;

    this->current_recalibration_time_ = this->current_operation == COVER_OPERATION_CLOSING
                                            ? this->recalibration_close_time_
                                            : this->recalibration_open_time_;
    this->current_actuator_activation_time_ = this->current_operation == COVER_OPERATION_CLOSING
                                                  ? this->actuator_activation_close_time_
                                                  : this->actuator_activation_open_time_;

    trig->trigger();
    this->last_recompute_time_ = now;

    this->fsm_state_ = STATE_MOVING;
    ESP_LOGD(TAG, "Transition to the moving state");
    return;
  }

  // moving state
  if (this->fsm_state_ == STATE_MOVING) {
    auto travel_time = now - this->last_recompute_time_;
    this->last_recompute_time_ = now;

    // Actuator activation time support.
    if (this->current_actuator_activation_time_ > 0) {
      if (travel_time <= this->current_actuator_activation_time_) {
        this->current_actuator_activation_time_ = this->current_actuator_activation_time_ - travel_time;
        return;
      } else {
        travel_time = travel_time - this->current_actuator_activation_time_;
        this->current_actuator_activation_time_ = 0;
      }
    }

    float dir_factor = this->current_operation == COVER_OPERATION_CLOSING ? -1.0 : 1.0;
    auto inertia_time =
        this->current_operation == COVER_OPERATION_CLOSING ? this->inertia_close_time_ : this->inertia_open_time_;

    if (inertia_time > 0 && this->inertia_ * dir_factor < 0.5f) {  // Inertia before movement
      auto inertia_step = dir_factor * travel_time / inertia_time;
      this->inertia_ += inertia_step;
      auto inertia_rest = this->inertia_ - clamp(this->inertia_, -0.5f, 0.5f);
      this->inertia_ = clamp(this->inertia_, -0.5f, 0.5f);

      if (!inertia_rest)
        return;                                                // The movement has not yet started.
      travel_time = dir_factor * inertia_rest * inertia_time;  // Actual movement time, taking inertia into account.
    }

    auto tilt_time =
        this->current_operation == COVER_OPERATION_CLOSING ? this->tilt_close_duration_ : this->tilt_open_duration_;

    if (tilt_time > 0 && (this->tilt - 0.5f) * dir_factor < 0.5f) {  // Tilting before movement.
      auto tilt_step = dir_factor * travel_time / tilt_time;
      this->tilt += tilt_step;
      auto tilt_rest = this->tilt - 0.5f - clamp(this->tilt - 0.5f, -0.5f, 0.5f);
      this->tilt = clamp(this->tilt, 0.0f, 1.0f);

      if (this->target_position_ == TARGET_NONE && this->is_at_target_tilt_()) {  // Only tilting w/o position change
        this->last_recompute_time_ = now;
        this->target_tilt_ = TARGET_NONE;
        this->last_publish_time_ = now;
        this->tilt = this->round_position(this->tilt);

        // If the cover is in extreme positions, perform recalibration.
        if (this->current_recalibration_time_ > 0 && this->is_at_extreme_position_()) {
          this->fsm_state_ = STATE_CALIBRATING;
          this->publish_state(false);
          ESP_LOGD(TAG, "Transition to the calibration state");
        } else {
          this->fsm_state_ = STATE_STOPPING;
          ESP_LOGD(TAG, "Transition to the stopping state");
        }

        return;  // Only tilting w/o position change, so there is no need to recompute the position.
      }

      if (now - this->last_publish_time_ > ((tilt_time / 5) > 1000 ? 1000 : (tilt_time / 5))) {
        this->publish_state(false);
        this->last_publish_time_ = now;
      }

      if (!tilt_rest)
        return;  // The movement has not started yet.

      travel_time = dir_factor * tilt_rest * tilt_time;  // Actual movement time, taking tilt into account.
    }

    auto move_time = this->current_operation == COVER_OPERATION_CLOSING ? this->close_duration_ : this->open_duration_;

    if ((this->position - 0.5f) * dir_factor < 0.5f) {
      auto move_step = dir_factor * travel_time / move_time;
      this->position += move_step;
      this->position = clamp(this->position, 0.0f, 1.0f);
    }

    if (this->is_at_target_position_()) {
      this->last_recompute_time_ = now;
      this->target_position_ = TARGET_NONE;
      this->last_publish_time_ = now;
      this->position = this->round_position(this->position);

      // If the cover is in extreme positions, perform recalibration.
      if (this->current_recalibration_time_ > 0 && this->is_at_extreme_position_()) {
        this->fsm_state_ = STATE_CALIBRATING;
        this->publish_state(false);
        ESP_LOGD(TAG, "Transition to the calibration state");
      } else {
        this->fsm_state_ = STATE_STOPPING;
        ESP_LOGD(TAG, "Transition to the stopping state");
      }
    }

    if (now - this->last_publish_time_ > 1000) {
      this->publish_state(false);
      this->last_publish_time_ = now;
    }
  }
}

float TimeBasedTiltCover::get_setup_priority() const { return setup_priority::DATA; }
CoverTraits TimeBasedTiltCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_tilt(this->tilt_close_duration_ != 0 && this->tilt_open_duration_ != 0);
  traits.set_supports_toggle(true);
  traits.set_supports_stop(true);
  traits.set_is_assumed_state(this->assumed_state_);
  return traits;
}
void TimeBasedTiltCover::control(const CoverCall &call) {
  if (call.get_stop()) {
    this->target_position_ = TARGET_NONE;
    this->target_tilt_ = TARGET_NONE;
    this->fsm_state_ = STATE_STOPPING;
    ESP_LOGD(TAG, "Transition to the stopping state");
    return;
  }

  if (call.get_position().has_value() || call.get_tilt().has_value()) {
    if (call.get_tilt().has_value()) {
      this->target_tilt_ = *call.get_tilt();
    } else {
      this->target_tilt_ = TARGET_NONE;
    }

    if (call.get_position().has_value()) {
      this->target_position_ = *call.get_position();
      if (this->target_position_ == COVER_CLOSED && this->close_sets_tilt_ && this->target_tilt_ == TARGET_NONE) {
        this->target_tilt_ = COVER_CLOSED;
      } else if (this->target_position_ == COVER_OPEN && this->open_sets_tilt_ && this->target_tilt_ == TARGET_NONE) {
        this->target_tilt_ = COVER_OPEN;
      }
    } else {
      this->target_position_ = TARGET_NONE;
    }

    if (this->fsm_state_ == STATE_MOVING) {
      auto direction = COVER_OPERATION_IDLE;
      if (this->target_position_ != TARGET_NONE && this->target_position_ != this->position) {
        direction = this->compute_direction(this->target_position_, this->position);
      } else if (this->target_tilt_ != TARGET_NONE && this->target_tilt_ != this->tilt) {
        direction = this->compute_direction(this->target_tilt_, this->tilt);
      }

      if (direction != this->current_operation) {
        this->fsm_state_ = STATE_STOPPING;
        ESP_LOGD(TAG, "Transition to the stopping state");
      }
    }

    if (this->fsm_state_ == STATE_CALIBRATING) {
      if (this->target_position_ != TARGET_NONE) {
        if ((this->position == COVER_CLOSED && this->target_position_ != COVER_CLOSED) ||
            (this->position == COVER_OPEN && this->target_position_ != COVER_OPEN)) {
          this->fsm_state_ = STATE_STOPPING;
          ESP_LOGD(TAG, "Transition to the stopping state");
        }
      }
      if (this->target_tilt_ != TARGET_NONE) {
        if ((this->tilt == COVER_CLOSED && this->target_tilt_ != COVER_CLOSED) ||
            (this->tilt == COVER_OPEN && this->target_tilt_ != COVER_OPEN)) {
          this->fsm_state_ = STATE_STOPPING;
          ESP_LOGD(TAG, "Transition to the stopping state");
        }
      }
    }
  }

  if (call.get_toggle().has_value()) {
    if (this->current_operation != COVER_OPERATION_IDLE) {
      this->fsm_state_ = STATE_STOPPING;
      ESP_LOGD(TAG, "Transition to the stopping state");
      this->target_position_ = TARGET_NONE;
      this->target_tilt_ = TARGET_NONE;
    } else {
      if (this->position == COVER_CLOSED && this->tilt == COVER_CLOSED) {
        this->target_position_ = COVER_OPEN;
      } else if (this->position == COVER_OPEN && this->tilt == COVER_OPEN) {
        this->target_position_ = COVER_CLOSED;
      } else if (this->last_operation_ == COVER_OPERATION_CLOSING) {
        if (this->position != COVER_OPEN) {
          this->target_position_ = COVER_OPEN;
        } else {
          this->target_tilt_ = COVER_OPEN;
        }
      } else {
        if (this->position != COVER_CLOSED) {
          this->target_position_ = COVER_CLOSED;
        } else {
          this->target_tilt_ = COVER_CLOSED;
        }
      }
    }
  }
}

}  // namespace esphome::time_based_tilt
