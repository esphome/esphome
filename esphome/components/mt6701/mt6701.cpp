#include "mt6701.h"
#include "esphome/core/log.h"

namespace esphome::mt6701 {

bool MT6701Component::read_encoder() {
  if (this->is_failed() || this->suspend_sampling_)
    return false;

  uint16_t count;
  if (!this->read_count(count)) {
    if (this->consecutive_errors_ < MAX_CONSECUTIVE_ERRORS && ++this->consecutive_errors_ == MAX_CONSECUTIVE_ERRORS)
      this->status_set_warning(LOG_STR("repeated read/CRC errors"));
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
