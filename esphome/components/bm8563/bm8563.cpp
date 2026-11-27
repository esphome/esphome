#include "bm8563.h"

#include <cinttypes>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::bm8563 {

ESPHOME_LOG_TAG(TAG, "bm8563");

static constexpr uint8_t CONTROL_STATUS_1_REG = 0x00;
static constexpr uint8_t CONTROL_STATUS_2_REG = 0x01;
static constexpr uint8_t CALENDAR_REG = 0x02;
static constexpr uint8_t CALENDAR_LEN = 7;
static constexpr uint8_t TIMER_CONTROL_REG = 0x0E;
static constexpr uint8_t TIMER_VALUE_REG = 0x0F;
static constexpr uint8_t CLOCK_1_HZ = 0x82;
static constexpr uint8_t CLOCK_1_60_HZ = 0x83;
// Maximum duration: 255 minutes (at 1/60 Hz) = 15300 seconds
static constexpr uint32_t MAX_TIMER_DURATION_S = 255 * 60;

void BM8563::setup() {
  if (!this->setup_check(this->write_byte_16(CONTROL_STATUS_1_REG, 0)))
    return;
}

void BM8563::update() { this->read_time(); }

void BM8563::dump_config() {
  ESP_LOGCONFIG(TAG, "BM8563:");
  LOG_I2C_DEVICE(this);
}

void BM8563::start_timer(uint32_t duration_s) {
  this->clear_irq_();
  this->set_timer_irq_(duration_s);
}

void BM8563::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }

  ESP_LOGD(TAG, "Writing time: %i-%i-%i %i, %i:%i:%i", now.year, now.month, now.day_of_month, now.day_of_week, now.hour,
           now.minute, now.second);
  uint8_t raw[CALENDAR_LEN];
  time::encode_bcd_calendar<time::PCF_BCD_CALENDAR>(now, raw);
  this->write_register_(CALENDAR_REG, raw, CALENDAR_LEN);
}

void BM8563::read_time() {
  uint8_t raw[CALENDAR_LEN];
  if (this->read_register(CALENDAR_REG, raw, CALENDAR_LEN) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to read the time");
    return;
  }
  const ESPTime rtc_time = time::decode_bcd_calendar<time::PCF_BCD_CALENDAR>(raw);
  ESP_LOGD(TAG, "Read time: %i-%i-%i %i, %i:%i:%i", rtc_time.year, rtc_time.month, rtc_time.day_of_month,
           rtc_time.day_of_week, rtc_time.hour, rtc_time.minute, rtc_time.second);
  if (!rtc_time.is_valid(/*check_day_of_week=*/true, /*check_day_of_year=*/false)) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void BM8563::write_byte_(uint8_t reg, uint8_t value) {
  if (!this->write_byte(reg, value)) {
    ESP_LOGE(TAG, "Failed to write byte 0x%02X with value 0x%02X", reg, value);
  }
}

void BM8563::write_register_(uint8_t reg, const uint8_t *data, size_t len) {
  if (auto error = this->write_register(reg, data, len); error != i2c::ErrorCode::NO_ERROR) {
    ESP_LOGE(TAG, "Failed to write register 0x%02X with %zu bytes", reg, len);
  }
}

optional<uint8_t> BM8563::read_register_(uint8_t reg) {
  uint8_t data;
  if (auto error = this->read_register(reg, &data, 1); error != i2c::ErrorCode::NO_ERROR) {
    ESP_LOGE(TAG, "Failed to read register 0x%02X", reg);
    return {};
  }
  return data;
}

void BM8563::set_timer_irq_(uint32_t duration_s) {
  ESP_LOGI(TAG, "Timer Duration: %" PRIu32 " s", duration_s);

  if (duration_s > MAX_TIMER_DURATION_S) {
    ESP_LOGW(TAG, "Timer duration %" PRIu32 " s exceeds maximum %" PRIu32 " s", duration_s, MAX_TIMER_DURATION_S);
    return;
  }

  if (duration_s > 255) {
    uint8_t duration_minutes = duration_s / 60;
    this->write_byte_(TIMER_VALUE_REG, duration_minutes);
    this->write_byte_(TIMER_CONTROL_REG, CLOCK_1_60_HZ);
  } else {
    this->write_byte_(TIMER_VALUE_REG, duration_s);
    this->write_byte_(TIMER_CONTROL_REG, CLOCK_1_HZ);
  }

  auto maybe_ctrl_status_2 = this->read_register_(CONTROL_STATUS_2_REG);
  if (!maybe_ctrl_status_2.has_value()) {
    ESP_LOGE(TAG, "Failed to read CONTROL_STATUS_2_REG");
    return;
  }
  uint8_t ctrl_status_2_reg_value = maybe_ctrl_status_2.value();
  ctrl_status_2_reg_value |= (1 << 0);
  ctrl_status_2_reg_value &= ~(1 << 7);
  this->write_byte_(CONTROL_STATUS_2_REG, ctrl_status_2_reg_value);
}

void BM8563::clear_irq_() {
  auto maybe_data = this->read_register_(CONTROL_STATUS_2_REG);
  if (!maybe_data.has_value()) {
    ESP_LOGE(TAG, "Failed to read CONTROL_STATUS_2_REG");
    return;
  }
  uint8_t data = maybe_data.value();
  this->write_byte_(CONTROL_STATUS_2_REG, data & 0xf3);
}

void BM8563::disable_irq_() {
  this->clear_irq_();
  auto maybe_data = this->read_register_(CONTROL_STATUS_2_REG);
  if (!maybe_data.has_value()) {
    ESP_LOGE(TAG, "Failed to read CONTROL_STATUS_2_REG");
    return;
  }
  uint8_t data = maybe_data.value();
  this->write_byte_(CONTROL_STATUS_2_REG, data & 0xfc);
}

}  // namespace esphome::bm8563
