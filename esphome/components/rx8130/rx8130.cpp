#include "rx8130.h"
#include "esphome/core/log.h"

// https://download.epsondevice.com/td/pdf/app/RX8130CE_en.pdf

namespace esphome {
namespace rx8130 {
using namespace i2c;

static const char *const TAG = "rx8130";

void RX8130Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RX8130...");
  uint8_t clear_flags = 0x00;
  // Set digital offset to disabled with no offset
  if (this->write_register(RX8130_REG_DIG_OFFSET, &clear_flags, 1) != ERROR_OK) {
    this->mark_failed();
    return;
  }
  // Disable wakeup timers
  if (this->write_register(RX8130_REG_EXTEN, &clear_flags, 1) != ERROR_OK) {
    this->mark_failed();
    return;
  }
  // Clear VLF flag in case there has been data loss
  if (this->write_register(RX8130_REG_FLAG, &clear_flags, 1) != ERROR_OK) {
    this->mark_failed();
    return;
  }
  // Clear test flag and disable interrupts
  if (this->write_register(RX8130_REG_CTRL0, &clear_flags, 1) != ERROR_OK) {
    this->mark_failed();
    return;
  }
  // Enable battery charging and switching
  uint8_t battery_flags = 0x30;
  if (this->write_register(RX8130_REG_CTRL1, &battery_flags, 1) != ERROR_OK) {
    this->mark_failed();
    return;
  }
  // Clear STOP bit
  this->stop(false);
}

void RX8130Component::update() { this->read_time(); }

void RX8130Component::dump_config() {
  ESP_LOGCONFIG(TAG, "RX8130:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with RX8130 failed!");
    return;
  }
  ESP_LOGCONFIG(TAG, "  DOFFS  %s", format_bin((uint8_t) this->reg(RX8130_REG_DIG_OFFSET)).c_str());
  ESP_LOGCONFIG(TAG, "  EXTEN  %s", format_bin((uint8_t) this->reg(RX8130_REG_EXTEN)).c_str());
  ESP_LOGCONFIG(TAG, "  FLAGS  %s", format_bin((uint8_t) this->reg(RX8130_REG_FLAG)).c_str());
  ESP_LOGCONFIG(TAG, "  CTRL0  %s", format_bin((uint8_t) this->reg(RX8130_REG_CTRL0)).c_str());
  ESP_LOGCONFIG(TAG, "  CTRL1  %s", format_bin((uint8_t) this->reg(RX8130_REG_CTRL1)).c_str());
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}

float RX8130Component::get_setup_priority() const { return setup_priority::DATA; }

void RX8130Component::read_time() {
  uint8_t date[7];
  if (this->read_register(RX8130_REG_SEC, date, 7) != ERROR_OK) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    this->mark_failed();
    return;
  }
  ESPTime rtc_time{
      .second = bcd2dec(date[0] & 0x7f),
      .minute = bcd2dec(date[1] & 0x7f),
      .hour = bcd2dec(date[2] & 0x3f),
      .day_of_week = bcd2dec(date[3] & 0x7f),
      .day_of_month = bcd2dec(date[4] & 0x3f),
      .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
      .month = bcd2dec(date[5] & 0x1f),
      .year = static_cast<uint16_t>(bcd2dec(date[6]) + 2000),
      .is_dst = false,  // not used
      .timestamp = 0    // overwritten by recalc_timestamp_utc(false)
  };
  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid()) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  ESP_LOGD(TAG, "Read UTC time: %04d-%02d-%02d %02d:%02d:%02d", rtc_time.year, rtc_time.month, rtc_time.day_of_month,
           rtc_time.hour, rtc_time.minute, rtc_time.second);
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void RX8130Component::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }
  uint8_t buff[7];
  buff[0] = dec2bcd(now.second);
  buff[1] = dec2bcd(now.minute);
  buff[2] = dec2bcd(now.hour);
  buff[3] = dec2bcd(now.day_of_week);
  buff[4] = dec2bcd(now.day_of_month);
  buff[5] = dec2bcd(now.month);
  buff[6] = dec2bcd(now.year % 100);
  this->stop(true);
  if (this->write_register(RX8130_REG_SEC, buff, 7) != ERROR_OK) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    this->mark_failed();
  } else {
    ESP_LOGD(TAG, "Wrote UTC time: %04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day_of_month, now.hour,
             now.minute, now.second);
  }
  this->stop(false);
}

uint8_t RX8130Component::bcd2dec(uint8_t val) { return (val >> 4) * 10 + (val & 0x0f); }

uint8_t RX8130Component::dec2bcd(uint8_t val) { return ((val / 10) << 4) + (val % 10); }

void RX8130Component::stop(bool stop) { this->reg(RX8130_REG_CTRL0) = stop ? RX8130_BIT_CTRL_STOP : 0x00; }

}  // namespace rx8130
}  // namespace esphome
