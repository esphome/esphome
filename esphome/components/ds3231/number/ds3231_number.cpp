#include "ds3231_number.h"

#if defined(USE_DS3231_AGING_OFFSET) || defined(USE_DS3231_REFRESH_INTERVAL)

#include "esphome/core/log.h"

#include <cmath>

namespace esphome::ds3231 {

static const char *const TAG = "ds3231.number";

#ifdef USE_DS3231_AGING_OFFSET
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
#endif  // USE_DS3231_AGING_OFFSET

#ifdef USE_DS3231_REFRESH_INTERVAL
void DS3231RefreshIntervalNumber::setup() {
  this->publish_state(this->parent_->get_update_interval() / 1000.0f);
  // ds3231.set_refresh_interval can change the interval behind our back; mirror it
  // so the shown value stays honest.
  this->set_interval(2000, [this]() {
    float seconds = this->parent_->get_update_interval() / 1000.0f;
    if (seconds != this->state) {
      this->publish_state(seconds);
    }
  });
}

void DS3231RefreshIntervalNumber::control(float value) {
  this->parent_->set_refresh_interval(static_cast<uint32_t>(lroundf(value * 1000.0f)));
  this->publish_state(value);
}

void DS3231RefreshIntervalNumber::dump_config() { LOG_NUMBER("  ", "DS3231 Refresh Interval", this); }
#endif  // USE_DS3231_REFRESH_INTERVAL

}  // namespace esphome::ds3231

#endif
