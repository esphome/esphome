#include "rx8111.h"
#include "esphome/core/log.h"

namespace esphome::rx8111 {

ESPHOME_LOG_TAG(TAG, "rx8111");

static constexpr uint8_t RX8111_REG_SEC = 0x10;
static constexpr uint8_t RX8111_REG_FLAG = 0x1E;
static constexpr uint8_t RX8111_REG_CONTROL = 0x1F;
static constexpr uint8_t RX8111_FLAG_POR = 0x80;
static constexpr uint8_t RX8111_FLAG_VLF = 0x02;
static constexpr uint8_t RX8111_FLAG_XST = 0x01;
static constexpr uint8_t RX8111_CTRL_STOP = 0x01;
// FLAG bits are write-zero-to-clear; keep UF/TF/AF/EVF even if they latch during the write.
static constexpr uint8_t RX8111_KEEP_EVENT_FLAGS = 0x3C;

static constexpr uint8_t bcd_to_uint8(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static constexpr uint8_t uint8_to_bcd(uint8_t val) { return ((val / 10) << 4) | (val % 10); }

void RX8111Component::setup() { this->read_time(); }

void RX8111Component::update() { this->read_time(); }

void RX8111Component::dump_config() {
  ESP_LOGCONFIG(TAG, "RX8111:");
  LOG_I2C_DEVICE(this);
  RealTimeClock::dump_config();
}

void RX8111Component::read_time() {
  uint8_t flags;
  if (!this->read_byte(RX8111_REG_FLAG, &flags)) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (flags & (RX8111_FLAG_VLF | RX8111_FLAG_XST | RX8111_FLAG_POR)) {
    ESP_LOGW(TAG, "RTC time invalidated by power loss or oscillator stop; write trusted time");
    this->status_set_warning(LOG_STR("RTC time invalidated; write trusted time"));
    return;
  }

  uint8_t buf[7];
  // I2C access freezes the RX8111 counter, yielding a coherent calendar snapshot.
  if (!this->read_bytes(RX8111_REG_SEC, buf, sizeof(buf))) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  static constexpr uint8_t CALENDAR_MASKS[7] = {0x7F, 0x7F, 0x3F, 0x00, 0x3F, 0x1F, 0xFF};
  for (size_t i = 0; i < sizeof(buf); i++) {
    if (i == 3)
      continue;  // Weekday uses one-hot encoding, not BCD.
    buf[i] &= CALENDAR_MASKS[i];
    if ((buf[i] & 0x0F) > 9 || (buf[i] >> 4) > 9) {
      ESP_LOGW(TAG, "Invalid BCD in RTC time");
      this->status_set_warning(LOG_STR("Invalid RTC time"));
      return;
    }
  }
  const uint8_t onehot = buf[3];
  if (onehot == 0 || onehot > 0x40 || (onehot & (onehot - 1)) != 0) {
    ESP_LOGW(TAG, "Invalid RTC weekday");
    this->status_set_warning(LOG_STR("Invalid RTC time"));
    return;
  }
  const uint8_t weekday = static_cast<uint8_t>(__builtin_ctz(onehot) + 1);
  ESPTime rtc_time{
      .second = bcd_to_uint8(buf[0]),
      .minute = bcd_to_uint8(buf[1]),
      .hour = bcd_to_uint8(buf[2]),
      .day_of_week = weekday,
      .day_of_month = bcd_to_uint8(buf[4]),
      .day_of_year = 1,
      .month = bcd_to_uint8(buf[5]),
      .year = static_cast<uint16_t>(bcd_to_uint8(buf[6]) + 2000),
      .is_dst = false,
      .timestamp = 0,
  };
  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid() || rtc_time.second >= 60) {
    ESP_LOGW(TAG, "Invalid RTC time, not syncing to system clock");
    this->status_set_warning(LOG_STR("Invalid RTC time"));
    return;
  }
  const auto calendar = ESPTime::from_epoch_utc(rtc_time.timestamp);
  if (calendar.year != rtc_time.year || calendar.month != rtc_time.month ||
      calendar.day_of_month != rtc_time.day_of_month || calendar.day_of_week != weekday) {
    ESP_LOGW(TAG, "Invalid RTC calendar date or weekday");
    this->status_set_warning(LOG_STR("Invalid RTC time"));
    return;
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "Read UTC time: %04d-%02d-%02d %02d:%02d:%02d", rtc_time.year, rtc_time.month, rtc_time.day_of_month,
           rtc_time.hour, rtc_time.minute, rtc_time.second);
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void RX8111Component::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid() || now.year > 2099) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC");
    return;
  }
  if (!this->set_stop_bit_(true)) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  uint8_t buf[7];
  buf[0] = uint8_to_bcd(now.second);
  buf[1] = uint8_to_bcd(now.minute);
  buf[2] = uint8_to_bcd(now.hour);
  buf[3] = 1 << (now.day_of_week - 1);
  buf[4] = uint8_to_bcd(now.day_of_month);
  buf[5] = uint8_to_bcd(now.month);
  buf[6] = uint8_to_bcd(static_cast<uint8_t>(now.year - 2000));
  const bool written = this->write_bytes(RX8111_REG_SEC, buf, sizeof(buf));
  // Always try to restart the counter, even after a failed calendar write.
  const bool restarted = this->set_stop_bit_(false);
  if (!written || !restarted) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (!this->write_byte(RX8111_REG_FLAG, RX8111_KEEP_EVENT_FLAGS)) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->status_clear_warning();
  ESP_LOGD(TAG, "Wrote UTC time: %04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day_of_month, now.hour,
           now.minute, now.second);
}

bool RX8111Component::set_stop_bit_(bool stop) {
  uint8_t control;
  if (!this->read_byte(RX8111_REG_CONTROL, &control))
    return false;
  if (stop) {
    control |= RX8111_CTRL_STOP;
  } else {
    control &= ~RX8111_CTRL_STOP;
  }
  return this->write_byte(RX8111_REG_CONTROL, control);
}

}  // namespace esphome::rx8111
