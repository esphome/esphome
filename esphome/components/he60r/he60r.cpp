#include "he60r.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace he60r {

static const char *const TAG = "he60r.cover";
static const uint8_t query_byte = 0x38;
static const uint8_t toggle_byte = 0x30;
static bool query_seen;
static uint8_t counter;

using namespace esphome::cover;

void HE60rCover::setup() {
  auto restore = this->restore_state_();

  if (restore.has_value()) {
    restore->apply(this);
    this->publish_state(false);
  } else {
    // if no other information, assume half open
    this->position = 0.5f;
  }
  this->current_operation = COVER_OPERATION_IDLE;
  this->last_recompute_time_ = this->start_dir_time_ = millis();
}

CoverTraits HE60rCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(false);
  return traits;
}

void HE60rCover::dump_config() {
  LOG_COVER("", "HE60R Cover", this);
  this->check_uart_settings(1200, 1, uart::UART_CONFIG_PARITY_EVEN, 8);
  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
  if (this->max_duration_ < UINT32_MAX) {
    ESP_LOGCONFIG(TAG, "  Max Duration: %.1fs", this->max_duration_ / 1e3f);
    auto restore = this->restore_state_();
    if (restore.has_value())
      ESP_LOGCONFIG(TAG, "  Saved position %d%%", (int) (restore->position * 100.f));
  }
}

void HE60rCover::endstop_reached_(CoverOperation operation) {
  const uint32_t now = millis();

  this->set_current_operation_(COVER_OPERATION_IDLE);
  auto new_position = operation == COVER_OPERATION_OPENING ? COVER_OPEN : COVER_CLOSED;
  if (new_position != this->position || this->current_operation != COVER_OPERATION_IDLE) {
    this->position = new_position;
    this->current_operation = COVER_OPERATION_IDLE;
    if (this->last_command_ == operation) {
      float dur = (now - this->start_dir_time_) / 1e3f;
      ESP_LOGD(TAG, "'%s' - %s endstop reached. Took %.1fs.", this->name_.c_str(),
               operation == COVER_OPERATION_OPENING ? "Open" : "Close", dur);
    }
    this->publish_state();
  }
}

void HE60rCover::set_current_operation_(cover::CoverOperation operation) {
  if (this->current_operation != operation) {
    this->current_operation = operation;
    if (operation != COVER_OPERATION_IDLE)
      this->last_recompute_time_ = millis();
    this->publish_state();
  }
}

void HE60rCover::process_rx_(uint8_t data) {
  ESP_LOGV(TAG, "Process RX data %X", data);
  if (!query_seen) {
    query_seen = data == query_byte;
    if (!query_seen)
      ESP_LOGD(TAG, "RX Byte %02X", data);
    return;
  }
  switch (data) {
    case 0xB5:  // at closed endstop, jammed?
    case 0xF5:  // at closed endstop, jammed?
    case 0x55:  // at closed endstop
      this->next_direction_ = COVER_OPERATION_OPENING;
      this->endstop_reached_(COVER_OPERATION_CLOSING);
      break;

    case 0x52:  // at opened endstop
      this->next_direction_ = COVER_OPERATION_CLOSING;
      this->endstop_reached_(COVER_OPERATION_OPENING);
      break;

    case 0x51:  // travelling up after encountering obstacle
    case 0x01:  // travelling up
    case 0x11:  // travelling up, triggered by remote
      this->set_current_operation_(COVER_OPERATION_OPENING);
      this->next_direction_ = COVER_OPERATION_IDLE;
      break;

    case 0x44:  // travelling down
    case 0x14:  // travelling down, triggered by remote
      this->next_direction_ = COVER_OPERATION_IDLE;
      this->set_current_operation_(COVER_OPERATION_CLOSING);
      break;

    case 0x86:  //  Stopped, jammed?
    case 0x16:  // stopped midway while opening, by remote
    case 0x06:  // stopped midway while opening
      this->next_direction_ = COVER_OPERATION_CLOSING;
      this->set_current_operation_(COVER_OPERATION_IDLE);
      break;

    case 0x10:  // stopped midway while closing, by remote
    case 0x00:  // stopped midway while closing
      this->next_direction_ = COVER_OPERATION_OPENING;
      this->set_current_operation_(COVER_OPERATION_IDLE);
      break;

    default:
      break;
  }
}

void HE60rCover::update() {
  if (toggles_needed_ != 0) {
    if ((counter++ & 0x3) == 0) {
      toggles_needed_--;
      ESP_LOGD(TAG, "Writing byte 0x30, still needed=%d", toggles_needed_);
      this->write_byte(toggle_byte);
    } else {
      this->write_byte(query_byte);
    }
  } else {
    this->write_byte(query_byte);
    counter = 0;
  }
  if (this->current_operation != COVER_OPERATION_IDLE) {
    const uint32_t now = millis();

    // Recompute position unless idle
    this->recompute_position_();

    // if we initiated the move, check if we reached position or max time
    // (stoping from endstop sensor is handled in callback)
    if (this->last_command_ != COVER_OPERATION_IDLE) {
      if (this->is_at_target_()) {
        this->start_direction_(COVER_OPERATION_IDLE);
      } else if (now - this->start_dir_time_ > this->max_duration_) {
        ESP_LOGW(TAG, "'%s' - Max duration reached. Stopping cover.", this->name_.c_str());
        this->start_direction_(COVER_OPERATION_IDLE);
      }
    }
  }
}

void HE60rCover::loop() {
  uint8_t data;

  while (this->available() > 0)
    if (this->read_byte(&data)) {
      this->process_rx_(data);
    }
}

void HE60rCover::control(const CoverCall &call) {
  // stop action logic
  if (call.get_stop()) {
    this->start_direction_(COVER_OPERATION_IDLE);
  } else if (call.get_toggle().has_value()) {
    // toggle action logic: OPEN - STOP - CLOSE
    if (this->last_command_ != COVER_OPERATION_IDLE) {
      this->start_direction_(COVER_OPERATION_IDLE);
    } else {
      this->toggles_needed_++;
    }
  } else if (call.get_position().has_value()) {
    // go to position action
    auto pos = *call.get_position();
    // are we at the target?
    if (pos == this->position) {
      this->start_direction_(COVER_OPERATION_IDLE);
    } else {
      this->target_position_ = pos;
      this->start_direction_(pos < this->position ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING);
    }
  }
}

bool HE60rCover::is_at_target_() const {
  // if initiated externally, current operation might be different from
  // operation that was triggered, thus evaluate position against what was asked

  switch (this->last_command_) {
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
void HE60rCover::start_direction_(CoverOperation dir) {
  this->last_command_ = dir;
  if (this->current_operation == dir)
    return;
  ESP_LOGD(TAG, "'%s' - Direction '%s' requested.", this->name_.c_str(),
           dir == COVER_OPERATION_OPENING   ? "OPEN"
           : dir == COVER_OPERATION_CLOSING ? "CLOSE"
                                            : "STOP");

  if (dir == this->next_direction_) {
    // either moving and needs to stop, or stopped and will move correctly on one trigger
    this->toggles_needed_ = 1;
  } else {
    // if stopped, but will go the wrong way, need 3 triggers.
    if (this->current_operation == COVER_OPERATION_IDLE)
      this->toggles_needed_ = 3;
    else
      this->toggles_needed_ = 2;  // just stop and reverse
    // must be moving, but the wrong way
    ESP_LOGD(TAG, "'%s' - Reversing direction.", this->name_.c_str());
  }
  this->start_dir_time_ = millis();
}

void HE60rCover::recompute_position_() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  const uint32_t now = millis();
  float dir;
  float action_dur;
  float min_pos = COVER_CLOSED + 0.01f;
  float max_pos = COVER_OPEN - 0.01f;

  // endstop sensors update position from their callbacks, and sets the fully open/close value
  // If we have endstop, estimation never reaches the fully open/closed state.
  // but if movement continues past corresponding endstop (inertia), keep the fully open/close state

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
      dir = 0;
      action_dur = 1;
      return;
  }

  if (now > this->last_recompute_time_) {
    auto diff = now - last_recompute_time_;
    auto delta = dir * diff / action_dur;
    this->position = clamp(delta + this->position, min_pos, max_pos);
    ESP_LOGD(TAG, "Recompute %dms, dir=%f, action_dur=%d, delta=%f, pos=%f", (int) diff, dir, action_dur, delta,
             this->position);
    this->last_recompute_time_ = now;
    this->publish_state();
  }
}

}  // namespace he60r
}  // namespace esphome
