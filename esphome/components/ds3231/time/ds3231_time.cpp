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
  uint8_t raw[DS3231Component::CALENDAR_LEN];
  if (!this->status_comm_result(this->parent_->read_calendar(raw), LOG_STR("No valid time from the RTC")))
    return;
  const ESPTime rtc_time = time::decode_bcd_calendar<time::DS_BCD_CALENDAR>(raw);
  ESP_LOGD(TAG, "Read UTC time: %04u-%02u-%02u %02u:%02u:%02u (day of week %u)", rtc_time.year, rtc_time.month,
           rtc_time.day_of_month, rtc_time.hour, rtc_time.minute, rtc_time.second, rtc_time.day_of_week);
  // The day of week is a free-running counter on the DS3231 and may have been set with a different
  // numbering by other software, so it is not used to validate the time.
  if (!this->status_comm_result(rtc_time.is_valid(/*check_day_of_week=*/false, /*check_day_of_year=*/false),
                                LOG_STR("Invalid RTC time")))
    return;
  this->synchronize_epoch_(rtc_time.timestamp);
}

void DS3231Time::write_time() {
  auto now = this->utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }
  if (now.year > 2199) {
    ESP_LOGE(TAG, "Year %u is out of the DS3231 range (2000-2199).", now.year);
    return;
  }
  uint8_t raw[DS3231Component::CALENDAR_LEN];
  time::encode_bcd_calendar<time::DS_BCD_CALENDAR>(now, raw);
  if (!this->status_comm_result(this->parent_->write_calendar(raw)))
    return;
  ESP_LOGD(TAG, "Wrote UTC time: %04u-%02u-%02u %02u:%02u:%02u", now.year, now.month, now.day_of_month, now.hour,
           now.minute, now.second);
}

}  // namespace esphome::ds3231
