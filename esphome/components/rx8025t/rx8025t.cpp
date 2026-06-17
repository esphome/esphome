#include "rx8025t.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://support.epson.biz/td/api/doc_check.php?dl=app_RX8025T

namespace esphome::rx8025t {

static const char *const TAG = "rx8025t";

void RX8025TComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RX8025T...");
  if (!this->read_rtc_()) {
    this->mark_failed();
    return;
  }

  // Check voltage low flag - indicates oscillator may have stopped
  if (this->rx8025t_.reg.vlf) {
    ESP_LOGW(TAG, "RX8025T VLF flag is set - Loss of oscillator detected. Time may be invalid.");
  }
}

void RX8025TComponent::update() { this->read_time(); }

void RX8025TComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RX8025T:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  RealTimeClock::dump_config();
}

float RX8025TComponent::get_setup_priority() const { return setup_priority::DATA; }

void RX8025TComponent::read_time() {
  if (!this->read_rtc_()) {
    return;
  }

  // Check voltage low flag - if set, time data is unreliable
  if (this->rx8025t_.reg.vlf) {
    ESP_LOGW(TAG, "RX8025T has VLF flag set - time data may be invalid, not syncing to system clock.");
    return;
  }

  ESPTime rtc_time{
      .second = uint8_t(this->rx8025t_.reg.second + 10u * this->rx8025t_.reg.second_10),
      .minute = uint8_t(this->rx8025t_.reg.minute + 10u * this->rx8025t_.reg.minute_10),
      .hour = uint8_t(this->rx8025t_.reg.hour + 10u * this->rx8025t_.reg.hour_10),
      .day_of_week = uint8_t(this->rx8025t_.reg.weekday + 1),  // ESPTime uses 1-7, RX8025T uses 0-6
      .day_of_month = uint8_t(this->rx8025t_.reg.day + 10u * this->rx8025t_.reg.day_10),
      .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
      .month = uint8_t(this->rx8025t_.reg.month + 10u * this->rx8025t_.reg.month_10),
      .year = uint16_t(this->rx8025t_.reg.year + 10u * this->rx8025t_.reg.year_10 + 2000),
      .is_dst = false,  // not used
      .timestamp = 0    // overwritten by recalc_timestamp_utc(false)
  };
  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid()) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void RX8025TComponent::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }

  this->rx8025t_.reg.year = (now.year - 2000) % 10;
  this->rx8025t_.reg.year_10 = (now.year - 2000) / 10 % 10;
  this->rx8025t_.reg.month = now.month % 10;
  this->rx8025t_.reg.month_10 = now.month / 10;
  this->rx8025t_.reg.day = now.day_of_month % 10;
  this->rx8025t_.reg.day_10 = now.day_of_month / 10;
  this->rx8025t_.reg.weekday = (now.day_of_week - 1) % 7;  // ESPTime uses 1-7, RX8025T uses 0-6
  this->rx8025t_.reg.hour = now.hour % 10;
  this->rx8025t_.reg.hour_10 = now.hour / 10;
  this->rx8025t_.reg.minute = now.minute % 10;
  this->rx8025t_.reg.minute_10 = now.minute / 10;
  this->rx8025t_.reg.second = now.second % 10;
  this->rx8025t_.reg.second_10 = now.second / 10;

  // Clear voltage low flag after writing valid time
  this->rx8025t_.reg.vlf = false;
  this->rx8025t_.reg.vdet = false;

  this->write_rtc_();
}

bool RX8025TComponent::read_rtc_() {
  if (!this->read_bytes(0, this->rx8025t_.raw, sizeof(this->rx8025t_.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read  %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u  VLF:%s VDET:%s", this->rx8025t_.reg.hour_10,
           this->rx8025t_.reg.hour, this->rx8025t_.reg.minute_10, this->rx8025t_.reg.minute,
           this->rx8025t_.reg.second_10, this->rx8025t_.reg.second, this->rx8025t_.reg.year_10, this->rx8025t_.reg.year,
           this->rx8025t_.reg.month_10, this->rx8025t_.reg.month, this->rx8025t_.reg.day_10, this->rx8025t_.reg.day,
           ONOFF(this->rx8025t_.reg.vlf), ONOFF(this->rx8025t_.reg.vdet));

  return true;
}

bool RX8025TComponent::write_rtc_() {
  if (!this->write_bytes(0, this->rx8025t_.raw, sizeof(this->rx8025t_.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u", this->rx8025t_.reg.hour_10,
           this->rx8025t_.reg.hour, this->rx8025t_.reg.minute_10, this->rx8025t_.reg.minute,
           this->rx8025t_.reg.second_10, this->rx8025t_.reg.second, this->rx8025t_.reg.year_10, this->rx8025t_.reg.year,
           this->rx8025t_.reg.month_10, this->rx8025t_.reg.month, this->rx8025t_.reg.day_10, this->rx8025t_.reg.day);
  return true;
}

}  // namespace esphome::rx8025t
