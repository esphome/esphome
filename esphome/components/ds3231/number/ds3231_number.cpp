#include "ds3231_number.h"

#ifdef USE_DS3231_AGING_OFFSET

#include "esphome/core/log.h"

#include <cmath>

namespace esphome::ds3231 {

static const char *const TAG = "ds3231.number";

void DS3231AgingOffsetNumber::setup() {
  int8_t offset;
  if (this->parent_->read_aging_offset(offset)) {
    this->publish_state(offset);
  }
}

void DS3231AgingOffsetNumber::control(float value) {
  auto offset = static_cast<int8_t>(lroundf(value));
  if (this->parent_->set_aging_offset(offset)) {
    this->publish_state(offset);
  } else {
    ESP_LOGW(TAG, "Setting aging offset failed.");
  }
}

void DS3231AgingOffsetNumber::dump_config() { LOG_NUMBER("  ", "DS3231 Aging Offset", this); }

}  // namespace esphome::ds3231

#endif  // USE_DS3231_AGING_OFFSET
