#include "rx8025t.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://support.epson.biz/td/api/doc_check.php?dl=app_RX8025T

namespace esphome::rx8025t {

static constexpr uint8_t RX8025T_REG_SEC = 0x00;
static constexpr uint8_t RX8025T_REG_FLAG = 0x0E;
static constexpr uint8_t RX8025T_FLAG_VDET = 0x01;
static constexpr uint8_t RX8025T_FLAG_VLF = 0x02;

static const char *const TAG = "rx8025t";

constexpr uint8_t bcd2dec(uint8_t val) { return (val >> 4) * 10 + (val & 0x0f); }
constexpr uint8_t dec2bcd(uint8_t val) { return ((val / 10) << 4) + (val % 10); }

void RX8025TComponent::setup() {
  uint8_t flags;
  if (!this->read_flags_(&flags)) {
    this->mark_failed();
    return;
  }

  if (flags & RX8025T_FLAG_VLF) {
    ESP_LOGW(TAG, "VLF flag is set - Loss of oscillator detected. Time may be invalid.");
  }
}

void RX8025TComponent::update() { this->read_time(); }

void RX8025TComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RX8025T:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  time::RealTimeClock::dump_config();
}

bool RX8025TComponent::read_flags_(uint8_t *flags) {
  if (!this->read_byte(RX8025T_REG_FLAG, flags)) {
    ESP_LOGE(TAG, "Can't read flag register.");
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  return true;
}

void RX8025TComponent::read_time() {
  uint8_t flags;
  if (!this->read_flags_(&flags)) {
    return;
  }

  uint8_t date[7];
  if (!this->read_bytes(RX8025T_REG_SEC, date, sizeof(date))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->status_clear_warning();

  if (flags & RX8025T_FLAG_VLF) {
    ESP_LOGW(TAG, "VLF flag is set - time data may be invalid, not syncing to system clock.");
    return;
  }

  ESPTime rtc_time{
      .second = bcd2dec(date[0] & 0x7f),
      .minute = bcd2dec(date[1] & 0x7f),
      .hour = bcd2dec(date[2] & 0x3f),
      .day_of_week = static_cast<uint8_t>((date[3] & 0x7f) ? __builtin_ctz(date[3] & 0x7f) + 1 : 1),
      .day_of_month = bcd2dec(date[4] & 0x3f),
      .day_of_year = 1,
      .month = bcd2dec(date[5] & 0x1f),
      .year = static_cast<uint16_t>(bcd2dec(date[6]) + 2000),
      .is_dst = false,
      .timestamp = 0,
  };
  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid(/*check_day_of_week=*/true, /*check_day_of_year=*/false)) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  ESP_LOGD(TAG, "Read UTC time: %04d-%02d-%02d %02d:%02d:%02d  VDET:%s", rtc_time.year, rtc_time.month,
           rtc_time.day_of_month, rtc_time.hour, rtc_time.minute, rtc_time.second, ONOFF(flags & RX8025T_FLAG_VDET));
  this->synchronize_epoch_(rtc_time.timestamp);
}

void RX8025TComponent::write_time() {
  auto now = this->utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }

  uint8_t buff[7];
  buff[0] = dec2bcd(now.second);
  buff[1] = dec2bcd(now.minute);
  buff[2] = dec2bcd(now.hour);
  buff[3] = 1 << (now.day_of_week - 1);
  buff[4] = dec2bcd(now.day_of_month);
  buff[5] = dec2bcd(now.month);
  buff[6] = dec2bcd(now.year % 100);
  if (!this->write_bytes(RX8025T_REG_SEC, buff, sizeof(buff))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  ESP_LOGD(TAG, "Wrote UTC time: %04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day_of_month, now.hour,
           now.minute, now.second);

  // Clear VLF and VDET flags via read-modify-write of flag register
  uint8_t flags;
  if (!this->read_flags_(&flags)) {
    return;
  }
  flags &= ~(RX8025T_FLAG_VLF | RX8025T_FLAG_VDET);
  if (!this->write_byte(RX8025T_REG_FLAG, flags)) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->status_clear_warning();
}

}  // namespace esphome::rx8025t
