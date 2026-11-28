#include "pcf8563.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://nl.mouser.com/datasheet/2/302/PCF8563-1127619.pdf

namespace esphome::pcf8563 {

ESPHOME_LOG_TAG(TAG, "PCF8563");

static constexpr uint8_t CONTROL_1_REG = 0x00;
static constexpr uint8_t CONTROL_1_STOP = 0x20;
static constexpr uint8_t CALENDAR_REG = 0x02;
static constexpr uint8_t CALENDAR_LEN = 7;
// Control_1, Control_2, then the calendar
static constexpr uint8_t READ_LEN = CALENDAR_REG + CALENDAR_LEN;
static constexpr uint8_t VOLTAGE_LOW_BIT = 0x80;  // In the seconds register

void PCF8563Component::setup() {
  uint8_t raw[READ_LEN];
  this->setup_check(this->read_bytes(CONTROL_1_REG, raw, READ_LEN));
}

void PCF8563Component::update() { this->read_time(); }

void PCF8563Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PCF8563:");
  LOG_I2C_DEVICE(this);
  RealTimeClock::dump_config();
}

void PCF8563Component::read_time() {
  uint8_t raw[READ_LEN];
  if (!this->read_bytes(CONTROL_1_REG, raw, READ_LEN)) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return;
  }
  if ((raw[CONTROL_1_REG] & CONTROL_1_STOP) != 0) {
    ESP_LOGW(TAG, "RTC halted, not syncing to system clock.");
    return;
  }
  if ((raw[CALENDAR_REG] & VOLTAGE_LOW_BIT) != 0) {
    ESP_LOGW(TAG, "Voltage-low flag set, RTC time may be invalid; not syncing to system clock.");
    return;
  }
  const ESPTime rtc_time = time::decode_bcd_calendar<time::PCF_BCD_CALENDAR>(&raw[CALENDAR_REG]);
  ESP_LOGD(TAG, "Read UTC time: %04u-%02u-%02u %02u:%02u:%02u", rtc_time.year, rtc_time.month, rtc_time.day_of_month,
           rtc_time.hour, rtc_time.minute, rtc_time.second);
  if (!rtc_time.is_valid(/*check_day_of_week=*/true, /*check_day_of_year=*/false)) {
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
  // The voltage-low flag is written clear with the seconds
  uint8_t raw[CALENDAR_LEN];
  time::encode_bcd_calendar<time::PCF_BCD_CALENDAR>(now, raw);
  if (!this->update_register(CONTROL_1_REG, CONTROL_1_STOP, 0) || !this->write_bytes(CALENDAR_REG, raw, CALENDAR_LEN)) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return;
  }
  ESP_LOGD(TAG, "Wrote UTC time: %04u-%02u-%02u %02u:%02u:%02u", now.year, now.month, now.day_of_month, now.hour,
           now.minute, now.second);
}

}  // namespace esphome::pcf8563
