#include "pcf8563.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://nl.mouser.com/datasheet/2/302/PCF8563-1127619.pdf

namespace esphome {
namespace pcf8563 {

static const char *const TAG = "PCF8563";

void PCF8563Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PCF8563...");
  if (!this->read_rtc_()) {
    this->mark_failed();
  }
}

void PCF8563Component::update() { this->read_time(); }

void PCF8563Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PCF8563:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with PCF8563 failed!");
  }
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}

float PCF8563Component::get_setup_priority() const { return setup_priority::DATA; }

void PCF8563Component::read_time() {
  if (!this->read_rtc_()) {
    return;
  }
  if (pcf8563_.reg.stop) {
    ESP_LOGW(TAG, "RTC halted, not syncing to system clock.");
    return;
  }
  ESPTime rtc_time{
      .second = uint8_t(pcf8563_.reg.second + 10 * pcf8563_.reg.second_10),
      .minute = uint8_t(pcf8563_.reg.minute + 10u * pcf8563_.reg.minute_10),
      .hour = uint8_t(pcf8563_.reg.hour + 10u * pcf8563_.reg.hour_10),
      .day_of_week = uint8_t(pcf8563_.reg.weekday),
      .day_of_month = uint8_t(pcf8563_.reg.day + 10u * pcf8563_.reg.day_10),
      .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
      .month = uint8_t(pcf8563_.reg.month + 10u * pcf8563_.reg.month_10),
      .year = uint16_t(pcf8563_.reg.year + 10u * pcf8563_.reg.year_10 + 2000),
      .is_dst = false,  // not used
      .timestamp = 0,   // overwritten by recalc_timestamp_utc(false)
  };
  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid()) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void PCF8563Component::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }
  pcf8563_.reg.year = (now.year - 2000) % 10;
  pcf8563_.reg.year_10 = (now.year - 2000) / 10 % 10;
  pcf8563_.reg.month = now.month % 10;
  pcf8563_.reg.month_10 = now.month / 10;
  pcf8563_.reg.day = now.day_of_month % 10;
  pcf8563_.reg.day_10 = now.day_of_month / 10;
  pcf8563_.reg.weekday = now.day_of_week;
  pcf8563_.reg.hour = now.hour % 10;
  pcf8563_.reg.hour_10 = now.hour / 10;
  pcf8563_.reg.minute = now.minute % 10;
  pcf8563_.reg.minute_10 = now.minute / 10;
  pcf8563_.reg.second = now.second % 10;
  pcf8563_.reg.second_10 = now.second / 10;
  pcf8563_.reg.stop = false;

  this->write_rtc_();
}

bool PCF8563Component::read_rtc_() {
  if (!this->read_bytes(0, this->pcf8563_.raw, sizeof(this->pcf8563_.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read  %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u  STOP:%s CLKOUT:%0u", pcf8563_.reg.hour_10,
           pcf8563_.reg.hour, pcf8563_.reg.minute_10, pcf8563_.reg.minute, pcf8563_.reg.second_10, pcf8563_.reg.second,
           pcf8563_.reg.year_10, pcf8563_.reg.year, pcf8563_.reg.month_10, pcf8563_.reg.month, pcf8563_.reg.day_10,
           pcf8563_.reg.day, ONOFF(!pcf8563_.reg.stop), pcf8563_.reg.clkout_enabled);

  return true;
}

bool PCF8563Component::write_rtc_() {
  if (!this->write_bytes(0, this->pcf8563_.raw, sizeof(this->pcf8563_.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u  OSC:%s CLKOUT:%0u", pcf8563_.reg.hour_10,
           pcf8563_.reg.hour, pcf8563_.reg.minute_10, pcf8563_.reg.minute, pcf8563_.reg.second_10, pcf8563_.reg.second,
           pcf8563_.reg.year_10, pcf8563_.reg.year, pcf8563_.reg.month_10, pcf8563_.reg.month, pcf8563_.reg.day_10,
           pcf8563_.reg.day, ONOFF(!pcf8563_.reg.stop), pcf8563_.reg.clkout_enabled);
  return true;
}
}  // namespace pcf8563
}  // namespace esphome
