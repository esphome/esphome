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
  if (!this->status_comm_result(this->parent_->read_time(rtc_time), LOG_STR("No valid time from the RTC")))
    return;
  this->synchronize_epoch_(rtc_time.timestamp);
}

void DS3231Time::write_time() {
  auto now = this->utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }
  this->status_comm_result(this->parent_->write_time(now));
}

}  // namespace esphome::ds3231
