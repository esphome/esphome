#include "ds1307.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://datasheets.maximintegrated.com/en/ds/DS1307.pdf

namespace esphome::ds1307 {

ESPHOME_LOG_TAG(TAG, "ds1307");

static constexpr uint8_t CALENDAR_REG = 0x00;
static constexpr uint8_t CALENDAR_LEN = 7;
static constexpr uint8_t CLOCK_HALT_BIT = 0x80;  // In the seconds register

void DS1307Component::setup() {
  uint8_t raw[CALENDAR_LEN];
  this->setup_check(this->read_bytes(CALENDAR_REG, raw, CALENDAR_LEN));
}

void DS1307Component::update() { this->read_time(); }

void DS1307Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS1307:");
  LOG_I2C_DEVICE(this);
  RealTimeClock::dump_config();
}

void DS1307Component::read_time() {
  uint8_t raw[CALENDAR_LEN];
  if (!this->read_bytes(CALENDAR_REG, raw, CALENDAR_LEN)) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return;
  }
  if ((raw[0] & CLOCK_HALT_BIT) != 0) {
    ESP_LOGW(TAG, "RTC halted, not syncing to system clock.");
    return;
  }
  const ESPTime rtc_time = time::decode_bcd_calendar<time::DS_BCD_CALENDAR>(raw);
  ESP_LOGD(TAG, "Read UTC time: %04u-%02u-%02u %02u:%02u:%02u", rtc_time.year, rtc_time.month, rtc_time.day_of_month,
           rtc_time.hour, rtc_time.minute, rtc_time.second);
  if (!rtc_time.is_valid(/*check_day_of_week=*/true, /*check_day_of_year=*/false)) {
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
  // The clock halt bit is written clear, so this also starts a halted clock
  uint8_t raw[CALENDAR_LEN];
  time::encode_bcd_calendar<time::DS_BCD_CALENDAR>(now, raw);
  if (!this->write_bytes(CALENDAR_REG, raw, CALENDAR_LEN)) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return;
  }
  ESP_LOGD(TAG, "Wrote UTC time: %04u-%02u-%02u %02u:%02u:%02u", now.year, now.month, now.day_of_month, now.hour,
           now.minute, now.second);
}

}  // namespace esphome::ds1307
