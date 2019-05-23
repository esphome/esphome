#include "tilt_cover.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tilt {

static const char *TAG = "tilt.cover";

using namespace esphome::cover;

void TiltCover::dump_config() {
  LOG_COVER("", "Tilt Cover", this);
  ESP_LOGCONFIG(TAG, "  Tilt Closed Value: %.0f%%", this->get_tilt_closed_value() * 100.0);
  ESP_LOGCONFIG(TAG, "  Tilt Opened Value: %.0f%%", this->get_tilt_opened_value() * 100.0);
  ESP_LOGCONFIG(TAG, "  Tilt Close Speed: %.1f%%/s", this->tilt_close_speed_);
  ESP_LOGCONFIG(TAG, "  Tilt Open Speed: %.1f%%/s", this->tilt_open_speed_);
}
void TiltCover::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->tilt = 0.5f;
  }
}
void TiltCover::loop() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  const uint32_t now = millis();

  // Recompute tilt and trigger action every loop cycle
  this->recompute_tilt_();
  tilt_trigger_->trigger();

  if (this->current_operation != COVER_OPERATION_IDLE && this->is_at_target_()) {
    this->start_direction_(COVER_OPERATION_IDLE);
    this->publish_state();
  }

  // Send current tilt every second
  if (this->current_operation != COVER_OPERATION_IDLE && now - this->last_publish_time_ > 100) {
    this->publish_state(false);
    this->last_publish_time_ = now;
  }
}
float TiltCover::get_setup_priority() const { return setup_priority::DATA; }
CoverTraits TiltCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(false);
  traits.set_supports_tilt(true);
  traits.set_is_assumed_state(false);
  return traits;
}
void TiltCover::control(const CoverCall &call) {
  if (call.get_stop()) {
    this->start_direction_(COVER_OPERATION_IDLE);
    this->publish_state();
  }
  if (call.get_tilt().has_value()) {
    auto target = *call.get_tilt();
    if (target == this->tilt) {
      // already at target
    } else {
      auto op = target < this->tilt ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING;
      this->target_tilt_ = target;
      this->start_direction_(op);
    }
  }
}
void TiltCover::stop_prev_trigger_() {
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop();
    this->prev_command_trigger_ = nullptr;
  }
}
bool TiltCover::is_at_target_() const {
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
void TiltCover::start_direction_(CoverOperation dir) {
  if (dir == this->current_operation)
    return;

  this->recompute_tilt_();
  Trigger<> *trig;
  switch (dir) {
    case COVER_OPERATION_IDLE:
      trig = this->stop_trigger_;
      break;
    case COVER_OPERATION_OPENING:
      trig = this->tilt_trigger_;
      break;
    case COVER_OPERATION_CLOSING:
      trig = this->tilt_trigger_;
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
void TiltCover::recompute_tilt_() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  float dir;
  float action_speed;
  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      dir = 1.0f;
      action_speed = this->tilt_open_speed_;
      break;
    case COVER_OPERATION_CLOSING:
      dir = -1.0f;
      action_speed = this->tilt_close_speed_;
      break;
    default:
      return;
  }

  const uint32_t now = millis();
  if (action_speed >= 1e6) {
    // Special case if action_speed is configured as 'inf' just set tilt value immediately
    this->tilt = this->target_tilt_;
  } else {
    // action_speed has units %/s so divide by 100000.0 to get fraction per ms
    this->tilt += dir * (now - this->last_recompute_time_) * action_speed / 100000.0;
    this->tilt = clamp(this->tilt, 0.0f, 1.0f);
  }

  this->last_recompute_time_ = now;
}

}  // namespace tilt
}  // namespace esphome
