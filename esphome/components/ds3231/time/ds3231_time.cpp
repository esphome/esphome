#include "ds3231_time.h"
#include "esphome/core/log.h"

namespace esphome::ds3231 {

ESPHOME_LOG_TAG(TAG, "ds3231.time");

void DS3231Time::update() { this->read_time(); }

void DS3231Time::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3231 Time:");
  RealTimeClock::dump_config();
}

void DS3231Time::read_time() {
  ESPTime rtc_time;
  if (!this->parent_->read_time(rtc_time)) {
    ESP_LOGW(TAG, "Not syncing the system clock from the RTC.");
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
  this->synchronize_epoch_(rtc_time.timestamp);
}

void DS3231Time::write_time() {
  auto now = this->utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }
  if (this->parent_->write_time(now)) {
    this->status_clear_warning();
  } else {
    this->status_set_warning();
  }
}

}  // namespace esphome::ds3231
