#include "rx8130.h"
#include "esphome/core/log.h"

// https://download.epsondevice.com/td/pdf/app/RX8130CE_en.pdf

namespace esphome {
namespace rx8130 {

static const uint8_t RX8130_REG_SEC = 0x10;
static const uint8_t RX8130_REG_MIN = 0x11;
static const uint8_t RX8130_REG_HOUR = 0x12;
static const uint8_t RX8130_REG_WDAY = 0x13;
static const uint8_t RX8130_REG_MDAY = 0x14;
static const uint8_t RX8130_REG_MONTH = 0x15;
static const uint8_t RX8130_REG_YEAR = 0x16;
static const uint8_t RX8130_REG_EXTEN = 0x1C;
static const uint8_t RX8130_REG_FLAG = 0x1D;
static const uint8_t RX8130_REG_CTRL0 = 0x1E;
static const uint8_t RX8130_REG_CTRL1 = 0x1F;
static const uint8_t RX8130_REG_DIG_OFFSET = 0x30;
static const uint8_t RX8130_BIT_CTRL_STOP = 0x40;
static const uint8_t RX8130_BAT_FLAGS = 0x30;
static const uint8_t RX8130_CLEAR_FLAGS = 0x00;

static const char *const TAG = "rx8130";

constexpr uint8_t bcd2dec(uint8_t val) { return (val >> 4) * 10 + (val & 0x0f); }
constexpr uint8_t dec2bcd(uint8_t val) { return ((val / 10) << 4) + (val % 10); }

void RX8130Component::setup() {
  // Set digital offset to disabled with no offset
  if (this->write_register(RX8130_REG_DIG_OFFSET, &RX8130_CLEAR_FLAGS, 1) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
  // Disable wakeup timers
  if (this->write_register(RX8130_REG_EXTEN, &RX8130_CLEAR_FLAGS, 1) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
  // Clear VLF flag in case there has been data loss
  if (this->write_register(RX8130_REG_FLAG, &RX8130_CLEAR_FLAGS, 1) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
  // Clear test flag and disable interrupts
  if (this->write_register(RX8130_REG_CTRL0, &RX8130_CLEAR_FLAGS, 1) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
  // Enable battery charging and switching
  if (this->write_register(RX8130_REG_CTRL1, &RX8130_BAT_FLAGS, 1) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
  // Clear STOP bit
  this->stop_(false);
}

void RX8130Component::update() { this->read_time(); }

void RX8130Component::dump_config() {
  ESP_LOGCONFIG(TAG, "RX8130:");
  LOG_I2C_DEVICE(this);
  RealTimeClock::dump_config();
}

void RX8130Component::read_time() {
  uint8_t date[7];
  if (this->read_register(RX8130_REG_SEC, date, 7) != i2c::ERROR_OK) {
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
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
  this->stop_(true);
  if (this->write_register(RX8130_REG_SEC, buff, 7) != i2c::ERROR_OK) {
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
  } else {
    ESP_LOGD(TAG, "Wrote UTC time: %04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day_of_month, now.hour,
             now.minute, now.second);
  }
  this->stop_(false);
}

void RX8130Component::stop_(bool stop) {
  const uint8_t data = stop ? RX8130_BIT_CTRL_STOP : RX8130_CLEAR_FLAGS;
  if (this->write_register(RX8130_REG_CTRL0, &data, 1) != i2c::ERROR_OK) {
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
  }
}

}  // namespace rx8130
}  // namespace esphome
