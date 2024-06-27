#include "time_based_tilt_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace time_based_tilt {

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
  ESP_LOGCONFIG(TAG, "  Recalibration time: %.3fs", this->recalibration_time_ / 1e3f);
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

void TimeBasedTiltCover::loop() {
  if (this->fsm_state_ == STATE_IDLE && this->target_position_ == TARGET_NONE && this->target_tilt_ == TARGET_NONE)
    return;

  const uint32_t now = millis();

  // recalibrating in extreme postions
  if (this->fsm_state_ == STATE_CALIBRATING) {
    if (now - this->last_recompute_time_ >= this->recalibration_time_) {
      this->fsm_state_ = STATE_STOPPING;
    }
    return;
  }

  // STOPPING - determining the direction of the last movement and the stopping time. Needed to support interlocking
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
    this->last_operation_ = this->current_operation;
    this->current_operation = COVER_OPERATION_IDLE;
    this->publish_state();
    return;
  }

  // if the cover is not moving, check whether the new targets are set and if they are, compute move direction
  if (this->fsm_state_ == STATE_IDLE && (this->target_position_ != TARGET_NONE || this->target_tilt_ != TARGET_NONE)) {
    if (this->target_position_ != TARGET_NONE) {
      this->current_operation = this->compute_direction(this->target_position_, this->position);
    } else {
      this->current_operation = this->compute_direction(this->target_tilt_, this->tilt);
    }
    // interlocking support
    if (this->current_operation == this->interlocked_direction_ &&
        now - this->interlocked_time_ < this->interlock_wait_time_)
      return;

    Trigger<> *trig = this->current_operation == COVER_OPERATION_CLOSING ? this->close_trigger_ : this->open_trigger_;

    trig->trigger();
    this->last_recompute_time_ = now;

    this->fsm_state_ = STATE_MOVING;
    return;
  }

  // moving state
  if (this->fsm_state_ == STATE_MOVING) {
    auto travel_time = now - this->last_recompute_time_;
    this->last_recompute_time_ = now;

    float dir_factor = this->current_operation == COVER_OPERATION_CLOSING ? -1.0 : 1.0;
    auto inertia_time =
        this->current_operation == COVER_OPERATION_CLOSING ? this->inertia_close_time_ : this->inertia_open_time_;

    if (inertia_time > 0 && this->inertia_ * dir_factor < 0.5f) {  // inertia before movement
      auto inertia_step = dir_factor * travel_time / inertia_time;
      this->inertia_ += inertia_step;
      auto rest = this->inertia_ - clamp(this->inertia_, -0.5f, 0.5f);
      this->inertia_ = clamp(this->inertia_, -0.5f, 0.5f);

      if (!rest)
        return;                                        // the movement has not yet actually started
      travel_time = dir_factor * rest * inertia_time;  // actual movement time taking inertia into account
    }

    auto tilt_time =
        this->current_operation == COVER_OPERATION_CLOSING ? this->tilt_close_duration_ : this->tilt_open_duration_;

    if (tilt_time > 0 && (this->tilt - 0.5f) * dir_factor < 0.5f) {  // tilting before movement
      auto tilt_step = dir_factor * travel_time / tilt_time;
      this->tilt += tilt_step;
      auto rest = this->tilt - 0.5f - clamp(this->tilt - 0.5f, -0.5f, 0.5f);
      this->tilt = clamp(this->tilt, 0.0f, 1.0f);

      if (this->target_position_ == TARGET_NONE && this->is_at_target_tilt_()) {  // only tilting w/o position change
        this->last_recompute_time_ = now;
        this->target_tilt_ = TARGET_NONE;
        this->last_publish_time_ = now;

        // If the cover is in extreme positions, run recalibration
        if (this->recalibration_time_ > 0 &&
            (((this->position == COVER_CLOSED && (tilt_time == 0 || this->tilt == COVER_CLOSED)) ||
              (this->position == COVER_OPEN && (tilt_time == 0 || this->tilt == COVER_OPEN))))) {
          this->fsm_state_ = STATE_CALIBRATING;
          this->publish_state(false);
        } else {
          this->fsm_state_ = STATE_STOPPING;
        }

        return;  // only tilting w/o position change so no need to recompute position
      }

      if (now - this->last_publish_time_ > ((tilt_time / 5) > 1000 ? 1000 : (tilt_time / 5))) {
        this->publish_state(false);
        this->last_publish_time_ = now;
      }

      if (!rest)
        return;  // the movement has not yet actually started

      travel_time = dir_factor * rest * tilt_time;  // actual movement time taking tilt into account
    }

    auto move_time = this->current_operation == COVER_OPERATION_CLOSING ? this->close_duration_ : this->open_duration_;

    if (move_time > 0 && (this->position - 0.5f) * dir_factor < 0.5f) {
      auto move_step = dir_factor * travel_time / move_time;
      this->position += move_step;
      this->position = clamp(this->position, 0.0f, 1.0f);
    }

    if (this->is_at_target_position_()) {
      this->last_recompute_time_ = now;
      this->target_position_ = TARGET_NONE;
      this->last_publish_time_ = now;

      // If the cover is in extreme positions, run recalibration
      if (this->recalibration_time_ > 0 &&
          (((this->position == COVER_CLOSED && (tilt_time == 0 || this->tilt == COVER_CLOSED)) ||
            (this->position == COVER_OPEN && (tilt_time == 0 || this->tilt == COVER_OPEN))))) {
        this->fsm_state_ = STATE_CALIBRATING;
        this->publish_state(false);
      } else {
        this->fsm_state_ = STATE_STOPPING;
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
    return;
  }
  if (call.get_position().has_value() && call.get_tilt().has_value()) {
    auto pos = *call.get_position();
    auto til = *call.get_tilt();

    if (this->round_position(pos) == this->round_position(this->position))
      pos = TARGET_NONE;
    if (this->round_position(til) == this->round_position(this->tilt))
      til = TARGET_NONE;

    this->target_position_ = pos;
    this->target_tilt_ = til;

    if (this->fsm_state_ == STATE_MOVING) {
      auto direction = COVER_OPERATION_IDLE;
      if (this->target_position_ != TARGET_NONE && this->target_position_ != this->position) {
        direction = this->compute_direction(this->target_position_, this->position);
      } else if (this->target_tilt_ != TARGET_NONE && this->target_tilt_ != this->tilt) {
        direction = this->compute_direction(this->target_tilt_, this->tilt);
      }

      if (direction != this->current_operation) {
        this->fsm_state_ = STATE_STOPPING;
      }
    }
  } else if (call.get_position().has_value()) {
    auto pos = *call.get_position();

    if (pos == COVER_CLOSED && this->position == COVER_CLOSED && this->tilt != COVER_CLOSED) {
      pos = TARGET_NONE;
      this->target_tilt_ = COVER_CLOSED;
    } else if (pos == COVER_OPEN && this->position == COVER_OPEN && this->tilt != COVER_OPEN) {
      pos = TARGET_NONE;
      this->target_tilt_ = COVER_OPEN;
    } else if (this->round_position(pos) == this->round_position(this->position)) {
      pos = TARGET_NONE;
    }

    this->target_position_ = pos;

    if (this->fsm_state_ == STATE_MOVING) {
      auto direction = COVER_OPERATION_IDLE;
      if (this->target_position_ != TARGET_NONE && this->target_position_ != this->position) {
        direction = this->compute_direction(this->target_position_, this->position);
        this->target_tilt_ = TARGET_NONE;  // unset previous target tilt
      } else if (this->target_tilt_ != TARGET_NONE && this->target_tilt_ != this->tilt) {
        direction = this->compute_direction(this->target_tilt_, this->tilt);
      }

      if (direction != this->current_operation) {
        this->fsm_state_ = STATE_STOPPING;
      }
    }
  } else if (call.get_tilt().has_value()) {
    auto til = *call.get_tilt();
    if (this->round_position(til) == this->round_position(this->tilt)) {
      til = TARGET_NONE;
    }

    this->target_tilt_ = til;

    if (this->fsm_state_ == STATE_MOVING) {
      auto direction = COVER_OPERATION_IDLE;
      if (this->target_tilt_ != TARGET_NONE && this->target_tilt_ != this->tilt) {
        direction = this->compute_direction(this->target_tilt_, this->tilt);
        this->target_position_ = TARGET_NONE;
      }

      if (direction != this->current_operation) {
        this->fsm_state_ = STATE_STOPPING;
      }
    }
  }

  if (call.get_toggle().has_value()) {
    if (this->current_operation != COVER_OPERATION_IDLE) {
      this->fsm_state_ = STATE_STOPPING;
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

}  // namespace time_based_tilt
}  // namespace esphome
