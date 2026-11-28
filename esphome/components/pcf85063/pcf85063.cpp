#include "pcf85063.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://www.nxp.com/docs/en/data-sheet/PCF85063A.pdf

namespace esphome::pcf85063 {

ESPHOME_LOG_TAG(TAG, "pcf85063");

static constexpr uint8_t CONTROL_1_REG = 0x00;
static constexpr uint8_t CONTROL_1_12_24 = 0x02;  // Set for 12-hour mode
static constexpr uint8_t CALENDAR_REG = 0x04;
static constexpr uint8_t CALENDAR_LEN = 7;
// Control_1, Control_2, Offset, RAM_byte, then the calendar
static constexpr uint8_t READ_LEN = CALENDAR_REG + CALENDAR_LEN;
static constexpr uint8_t HOURS_REG = CALENDAR_REG + 2;
static constexpr uint8_t OSCILLATOR_STOP_BIT = 0x80;  // In the seconds register

void PCF85063Component::setup() {
  uint8_t raw[READ_LEN];
  this->setup_check(this->read_bytes(CONTROL_1_REG, raw, READ_LEN));
}

void PCF85063Component::update() { this->read_time(); }

void PCF85063Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PCF85063:");
  LOG_I2C_DEVICE(this);
  RealTimeClock::dump_config();
}

void PCF85063Component::read_time() {
  uint8_t raw[READ_LEN];
  if (!this->read_bytes(CONTROL_1_REG, raw, READ_LEN)) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return;
  }
  if ((raw[CALENDAR_REG] & OSCILLATOR_STOP_BIT) != 0) {
    ESP_LOGW(TAG, "RTC halted, not syncing to system clock.");
    return;
  }
  ESPTime rtc_time = time::decode_bcd_calendar<time::PCF_BCD_CALENDAR>(&raw[CALENDAR_REG]);
  if ((raw[CONTROL_1_REG] & CONTROL_1_12_24) != 0) {
    ESP_LOGD(TAG, "RTC in 12-hour mode, the next sync switches it to 24-hour mode");
    // The decode read the hours as 24-hour, so redo the hour and the timestamp
    rtc_time.hour = decode_hour(raw[HOURS_REG] & 0x0F, (raw[HOURS_REG] >> 4) & 0x03, true);
    rtc_time.recalc_timestamp_utc(false);
  }
  ESP_LOGD(TAG, "Read UTC time: %04u-%02u-%02u %02u:%02u:%02u", rtc_time.year, rtc_time.month, rtc_time.day_of_month,
           rtc_time.hour, rtc_time.minute, rtc_time.second);
  if (!rtc_time.is_valid(/*check_day_of_week=*/true, /*check_day_of_year=*/false)) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void PCF85063Component::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }
  // 24-hour mode first, so the hours are read as written; the oscillator stop flag is written clear with the seconds
  uint8_t raw[CALENDAR_LEN];
  time::encode_bcd_calendar<time::PCF_BCD_CALENDAR>(now, raw);
  if (!this->update_register(CONTROL_1_REG, CONTROL_1_12_24, 0) ||
      !this->write_bytes(CALENDAR_REG, raw, CALENDAR_LEN)) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return;
  }
  ESP_LOGD(TAG, "Wrote UTC time: %04u-%02u-%02u %02u:%02u:%02u", now.year, now.month, now.day_of_month, now.hour,
           now.minute, now.second);
}

}  // namespace esphome::pcf85063
