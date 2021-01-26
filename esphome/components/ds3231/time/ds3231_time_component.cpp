#include "esphome/core/log.h"

#include "ds3231_time_component.h"

namespace esphome {
namespace ds3231 {

static const char *TAG = "ds3231.time";

uint8_t dectobcd(const uint8_t val) { return ((val / 10 * 16) + (val % 10)); }

uint8_t bcdtodec(const uint8_t val) { return ((val / 16 * 10) + (val % 16)); }

void DS3231TimeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RTC Time:");
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}

time_t DS3231TimeComponent::timestamp_now() {
  time::ESPTime now = read_time_();
  return now.timestamp;
}

void DS3231TimeComponent::set_epoch_time(const uint32_t epoch) {
  time::ESPTime current_time = time::ESPTime::from_epoch_utc(epoch);
  write_time_(current_time);
}

void DS3231TimeComponent::update() { read_time_(); }

void DS3231TimeComponent::write_time_(const time::ESPTime tm) {
  if (tm.is_valid()) {
    uint8_t raw_data[7] = {0};

    raw_data[0] = dectobcd(tm.second);
    raw_data[1] = dectobcd(tm.minute);
    raw_data[2] = dectobcd(tm.hour);
    raw_data[3] = dectobcd(tm.day_of_week);
    raw_data[4] = dectobcd(tm.day_of_month);
    raw_data[5] = dectobcd(tm.month);
    raw_data[6] = dectobcd(tm.year - 2000);

    this->write_bytes(0x00, raw_data, 7);
  }
}

time::ESPTime DS3231TimeComponent::read_time_() {
  uint8_t raw_data[7] = {0};
  this->read_bytes(0x00, raw_data, 7);

  time::ESPTime now = {};
  now.second = bcdtodec(raw_data[0]);
  now.minute = bcdtodec(raw_data[1]);
  now.hour = bcdtodec(raw_data[2]);
  now.day_of_week = 1;
  now.day_of_month = bcdtodec(raw_data[4]);
  now.month = bcdtodec(raw_data[5] & 0x1F);
  now.year = 2000 + bcdtodec(raw_data[6]);
  now.day_of_year = 1;

  now.recalc_timestamp_utc(false);

  return now;
}

}  // namespace ds3231
}  // namespace esphome
