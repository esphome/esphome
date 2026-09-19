#include "ds3231_time.h"
#include "esphome/core/log.h"

namespace esphome::ds3231 {

static const char *const TAG = "ds3231.time";

void DS3231Time::update() { this->read_time(); }

void DS3231Time::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3231 time:");
  RealTimeClock::dump_config();
}

void DS3231Time::read_time() {
  if (this->parent_->get_oscillator_stopped()) {
    ESP_LOGW(TAG, "Clock lost power, not syncing to system clock.");
    return;
  }
  ESPTime rtc_time;
  if (!this->parent_->read_datetime(rtc_time)) {
    return;
  }
  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid(/*check_day_of_week=*/true, /*check_day_of_year=*/false)) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  this->synchronize_epoch_(rtc_time.timestamp);
}

void DS3231Time::write_time() {
  auto now = this->utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }
  if (this->parent_->write_datetime(now)) {
    ESP_LOGD(TAG, "Synced system clock to RTC.");
  }
}

}  // namespace esphome::ds3231
