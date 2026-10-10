#include "mt6701.h"
#include "esphome/core/log.h"

namespace esphome::mt6701 {

void MT6701Component::handle_read_error_() {
  if (this->consecutive_errors_ >= MAX_CONSECUTIVE_ERRORS)
    return;
  this->consecutive_errors_++;
  if (this->consecutive_errors_ == MAX_CONSECUTIVE_ERRORS) {
    // The message overload logs once on the transition and is a no-op while
    // the warning is already set.
    this->status_set_warning("repeated read/CRC errors");
  }
}

bool MT6701Component::read_encoder() {
  // Never sample a hub that failed setup (SSI noise can pass the 6-bit CRC), nor
  // one whose bus is suspended for EEPROM programming.
  if (this->is_failed() || this->suspend_sampling_)
    return false;

  uint16_t count;
  if (!this->read_count(count)) {
    this->handle_read_error_();
    return false;
  }
  if (this->consecutive_errors_ != 0) {
    this->consecutive_errors_ = 0;
    this->status_clear_warning();
  }

  this->count_ = count;
  return true;
}

}  // namespace esphome::mt6701
