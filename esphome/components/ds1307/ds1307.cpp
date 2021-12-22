#include "ds1307.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://datasheets.maximintegrated.com/en/ds/DS1307.pdf

namespace esphome {
namespace ds1307 {

static const char *const TAG = "ds1307";

void DS1307Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS1307...");
  if (!this->read_rtc_()) {
    this->mark_failed();
  }
}

void DS1307Component::update() { this->read_time(); }

void DS1307Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS1307:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with DS1307 failed!");
  }
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}

float DS1307Component::get_setup_priority() const { return setup_priority::DATA; }

void DS1307Component::read_time() {
  if (!this->read_rtc_()) {
    return;
  }
  if (ds1307_.reg.ch) {
    ESP_LOGW(TAG, "RTC halted, not syncing to system clock.");
    return;
  }
  time::ESPTime rtc_time{.second = uint8_t(ds1307_.reg.second + 10 * ds1307_.reg.second_10),
                         .minute = uint8_t(ds1307_.reg.minute + 10u * ds1307_.reg.minute_10),
                         .hour = uint8_t(ds1307_.reg.hour + 10u * ds1307_.reg.hour_10),
                         .day_of_week = uint8_t(ds1307_.reg.weekday),
                         .day_of_month = uint8_t(ds1307_.reg.day + 10u * ds1307_.reg.day_10),
                         .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
                         .month = uint8_t(ds1307_.reg.month + 10u * ds1307_.reg.month_10),
                         .year = uint16_t(ds1307_.reg.year + 10u * ds1307_.reg.year_10 + 2000)};
  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid()) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void DS1307Component::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }
  ds1307_.reg.year = (now.year - 2000) % 10;
  ds1307_.reg.year_10 = (now.year - 2000) / 10 % 10;
  ds1307_.reg.month = now.month % 10;
  ds1307_.reg.month_10 = now.month / 10;
  ds1307_.reg.day = now.day_of_month % 10;
  ds1307_.reg.day_10 = now.day_of_month / 10;
  ds1307_.reg.weekday = now.day_of_week;
  ds1307_.reg.hour = now.hour % 10;
  ds1307_.reg.hour_10 = now.hour / 10;
  ds1307_.reg.minute = now.minute % 10;
  ds1307_.reg.minute_10 = now.minute / 10;
  ds1307_.reg.second = now.second % 10;
  ds1307_.reg.second_10 = now.second / 10;
  ds1307_.reg.ch = false;

  this->write_rtc_();
}

bool DS1307Component::read_rtc_() {
  if (!this->read_bytes(0, this->ds1307_.raw, sizeof(this->ds1307_.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read  %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u  CH:%s RS:%0u SQWE:%s OUT:%s", ds1307_.reg.hour_10,
           ds1307_.reg.hour, ds1307_.reg.minute_10, ds1307_.reg.minute, ds1307_.reg.second_10, ds1307_.reg.second,
           ds1307_.reg.year_10, ds1307_.reg.year, ds1307_.reg.month_10, ds1307_.reg.month, ds1307_.reg.day_10,
           ds1307_.reg.day, ONOFF(ds1307_.reg.ch), ds1307_.reg.rs, ONOFF(ds1307_.reg.sqwe), ONOFF(ds1307_.reg.out));

  return true;
}

bool DS1307Component::write_rtc_() {
  if (!this->write_bytes(0, this->ds1307_.raw, sizeof(this->ds1307_.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u  CH:%s RS:%0u SQWE:%s OUT:%s", ds1307_.reg.hour_10,
           ds1307_.reg.hour, ds1307_.reg.minute_10, ds1307_.reg.minute, ds1307_.reg.second_10, ds1307_.reg.second,
           ds1307_.reg.year_10, ds1307_.reg.year, ds1307_.reg.month_10, ds1307_.reg.month, ds1307_.reg.day_10,
           ds1307_.reg.day, ONOFF(ds1307_.reg.ch), ds1307_.reg.rs, ONOFF(ds1307_.reg.sqwe), ONOFF(ds1307_.reg.out));
  return true;
}
}  // namespace ds1307
}  // namespace esphome
