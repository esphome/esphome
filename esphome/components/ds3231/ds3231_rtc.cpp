#include "ds3231.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://datasheets.maximintegrated.com/en/ds/DS1307.pdf

namespace esphome {
namespace ds3231 {

static const char *const TAG = "ds3231.rtc";

void DS3231RTC::dump_config() {
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}

void DS3231RTC::read_time() {
  if (!this->parent_->read_stat_()) {
    ESP_LOGE(TAG, "Failed to read status, not syncing to system clock.");
    return;
  }
  if (this->parent_->ds3231_.stat.reg.osc_stop) {
    ESP_LOGW(TAG, "RTC halted, not syncing to system clock.");
    return;
  }
  if (!this->parent_->read_rtc_()) {
    ESP_LOGE(TAG, "Failed to read rtc, not syncing to system clock.");
    return;
  }
  time::ESPTime rtc_time{
    .second = uint8_t(this->parent_->ds3231_.rtc.reg.second + 10 * this->parent_->ds3231_.rtc.reg.second_10),
    .minute = uint8_t(this->parent_->ds3231_.rtc.reg.minute + 10u * this->parent_->ds3231_.rtc.reg.minute_10),
    .hour = uint8_t(this->parent_->ds3231_.rtc.reg.hour + 10u * this->parent_->ds3231_.rtc.reg.hour_10),
    .day_of_week = uint8_t(this->parent_->ds3231_.rtc.reg.weekday),
    .day_of_month = uint8_t(this->parent_->ds3231_.rtc.reg.day + 10u * this->parent_->ds3231_.rtc.reg.day_10),
    .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
    .month = uint8_t(this->parent_->ds3231_.rtc.reg.month + 10u * this->parent_->ds3231_.rtc.reg.month_10),
    .year = uint16_t(this->parent_->ds3231_.rtc.reg.year + 10u * this->parent_->ds3231_.rtc.reg.year_10 + 2000)
  };
  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid()) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void DS3231RTC::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGW(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }
  if (!this->parent_->read_stat_()) {
    ESP_LOGE(TAG, "Failed to read status, not syncing to RTC.");
    return;
  }
  if (this->parent_->ds3231_.stat.reg.osc_stop) {
    this->parent_->ds3231_.stat.reg.osc_stop = false;
    if (!this->parent_->write_stat_()) {
      ESP_LOGE(TAG, "Failed to write status, not syncing to RTC.");
    }
  }
  this->parent_->ds3231_.rtc.reg.second = now.second % 10;
  this->parent_->ds3231_.rtc.reg.second_10 = now.second / 10;
  this->parent_->ds3231_.rtc.reg.minute = now.minute % 10;
  this->parent_->ds3231_.rtc.reg.minute_10 = now.minute / 10;
  this->parent_->ds3231_.rtc.reg.hour = now.hour % 10;
  this->parent_->ds3231_.rtc.reg.hour_10 = now.hour / 10;
  this->parent_->ds3231_.rtc.reg.weekday = now.day_of_week;
  this->parent_->ds3231_.rtc.reg.day = now.day_of_month % 10;
  this->parent_->ds3231_.rtc.reg.day_10 = now.day_of_month / 10;
  this->parent_->ds3231_.rtc.reg.month = now.month % 10;
  this->parent_->ds3231_.rtc.reg.month_10 = now.month / 10;
  this->parent_->ds3231_.rtc.reg.year = (now.year - 2000) % 10;
  this->parent_->ds3231_.rtc.reg.year_10 = (now.year - 2000) / 10 % 10;
  if (!this->parent_->write_rtc_()) {
    ESP_LOGE(TAG, "Failed to sync to RTC.");
  }
}

}  // namespace ds3231
}  // namespace esphome
