#include "he60r.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace he60r {

static const char *const TAG = "he60r.cover";
static const uint8_t QUERY_BYTE = 0x38;
static const uint8_t TOGGLE_BYTE = 0x30;

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
  this->set_interval(300, [this]() { this->update_(); });
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
  auto restore = this->restore_state_();
  if (restore.has_value())
    ESP_LOGCONFIG(TAG, "  Saved position %d%%", (int) (restore->position * 100.f));
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
  if (!this->query_seen_) {
    this->query_seen_ = data == QUERY_BYTE;
    if (!this->query_seen_)
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

void HE60rCover::update_() {
  if (this->toggles_needed_ != 0) {
    if ((this->counter_++ & 0x3) == 0) {
      this->toggles_needed_--;
      ESP_LOGD(TAG, "Writing byte 0x30, still needed=%" PRIu32, this->toggles_needed_);
      this->write_byte(TOGGLE_BYTE);
    } else {
      this->write_byte(QUERY_BYTE);
    }
  } else {
    this->write_byte(QUERY_BYTE);
    this->counter_ = 0;
  }
  if (this->current_operation != COVER_OPERATION_IDLE) {
    this->recompute_position_();

    // if we initiated the move, check if we reached the target position
    if (this->last_command_ != COVER_OPERATION_IDLE) {
      if (this->is_at_target_()) {
        this->start_direction_(COVER_OPERATION_IDLE);
      }
    }
  }
}

void HE60rCover::loop() {
  uint8_t data;

  while (this->available() > 0) {
    if (this->read_byte(&data)) {
      this->process_rx_(data);
    }
  }
}

void HE60rCover::control(const CoverCall &call) {
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

/**
 * Check if the cover has reached or passed the target position. This is used only
 * for partial open/close requests - endstops are used for full open/close.
 * @return True if the cover has reached or passed its target position. For full open/close target always return false.
 */
bool HE60rCover::is_at_target_() const {
  // equality of floats is fraught with peril - this is reliable since the values are 0.0 or 1.0 which are
  // exactly representable.
  if (this->target_position_ == COVER_OPEN || this->target_position_ == COVER_CLOSED)
    return false;
  // aiming for an intermediate position - exact comparison here will not work and we need to allow for overshoot
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
    if (this->current_operation == COVER_OPERATION_IDLE) {
      // if stopped, but will go the wrong way, need 3 triggers.
      this->toggles_needed_ = 3;
    } else {
      // just stop and reverse
      this->toggles_needed_ = 2;
    }
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

  if (now > this->last_recompute_time_) {
    auto diff = now - last_recompute_time_;
    auto delta = dir * diff / action_dur;
    // make sure our guesstimate never reaches full open or close.
    this->position = clamp(delta + this->position, COVER_CLOSED + 0.01f, COVER_OPEN - 0.01f);
    ESP_LOGD(TAG, "Recompute %dms, dir=%f, action_dur=%f, delta=%f, pos=%f", (int) diff, dir, action_dur, delta,
             this->position);
    this->last_recompute_time_ = now;
    this->publish_state();
  }
}

}  // namespace he60r
}  // namespace esphome
