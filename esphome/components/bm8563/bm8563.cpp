#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c_bus.h"
#include "bm8563.h"

namespace esphome {
namespace bm8563 {

static const char *TAG = "bm8563.sensor";

void BM8563::setup() {
  this->write_byte_16(0, 0);
  this->setup_complete_ = true;
}

void BM8563::update() {
  if (!this->setup_complete_) {
    return;
  }
  this->read_time();
}

void BM8563::dump_config() {
  ESP_LOGCONFIG(TAG, "BM8563:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  ESP_LOGCONFIG(TAG, "  setup_complete: %s", this->setup_complete_ ? "true" : "false");
}

void BM8563::start_timer(uint32_t timer_s) {
  if (this->setup_complete_) {
    this->clear_irq_();
    this->set_alarm_irq_(timer_s);
  }
}

void BM8563::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }

  BM8563TimeTypeDef b_m8563_time_struct{
      .hours = int8_t(now.hour),
      .minutes = int8_t(now.minute),
      .seconds = int8_t(now.second),
  };

  BM8563DateTypeDef b_m8563_date_struct{.day = int8_t(now.day_of_month),
                                        .week = int8_t(now.day_of_week),
                                        .month = int8_t(now.month),
                                        .year = int16_t(now.year)};

  this->set_time_(&b_m8563_time_struct);
  this->set_date_(&b_m8563_date_struct);
}

void BM8563::read_time() {
  BM8563TimeTypeDef b_m8563_time_struct;
  BM8563DateTypeDef b_m8563_date_struct;
  this->get_time_(&b_m8563_time_struct);
  this->get_date_(&b_m8563_date_struct);
  ESP_LOGD(TAG, "BM8563: %i-%i-%i %i, %i:%i:%i", b_m8563_date_struct.year, b_m8563_date_struct.month,
           b_m8563_date_struct.day, b_m8563_date_struct.week, b_m8563_time_struct.hours, b_m8563_time_struct.minutes,
           b_m8563_time_struct.seconds);

  ESPTime rtc_time{
      .second = uint8_t(b_m8563_time_struct.seconds),
      .minute = uint8_t(b_m8563_time_struct.minutes),
      .hour = uint8_t(b_m8563_time_struct.hours),
      .day_of_week = uint8_t(b_m8563_date_struct.week),
      .day_of_month = uint8_t(b_m8563_date_struct.day),
      .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
      .month = uint8_t(b_m8563_date_struct.month),
      .year = uint16_t(b_m8563_date_struct.year),
      .is_dst = false,  // ignored by recalc_timestamp_utc()
      .timestamp = 0    // result
  };
  rtc_time.recalc_timestamp_utc(false);
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

bool BM8563::get_volt_low_() {
  uint8_t data = this->read_reg_(0x02);
  return data & 0x80;  // RTCC_VLSEC_MASK
}

uint8_t BM8563::bcd2_to_byte_(uint8_t value) {
  uint8_t tmp = 0;
  tmp = ((uint8_t) (value & (uint8_t) 0xF0) >> (uint8_t) 0x4) * 10;
  return (tmp + (value & (uint8_t) 0x0F));
}

uint8_t BM8563::byte_to_bcd2_(uint8_t value) {
  uint8_t bcdhigh = 0;

  while (value >= 10) {
    bcdhigh++;
    value -= 10;
  }

  return ((uint8_t) (bcdhigh << 4) | value);
}

void BM8563::get_time_(BM8563TimeTypeDef *b_m8563_time_struct) {
  uint8_t buf[3] = {0};

  this->read_register(0x02, buf, 3);

  b_m8563_time_struct->seconds = this->bcd2_to_byte_(buf[0] & 0x7f);
  b_m8563_time_struct->minutes = this->bcd2_to_byte_(buf[1] & 0x7f);
  b_m8563_time_struct->hours = this->bcd2_to_byte_(buf[2] & 0x3f);
}

void BM8563::set_time_(BM8563TimeTypeDef *b_m8563_time_struct) {
  if (b_m8563_time_struct == NULL) {
    return;
  }
  uint8_t buf[3] = {this->byte_to_bcd2_(b_m8563_time_struct->seconds),
                    this->byte_to_bcd2_(b_m8563_time_struct->minutes), this->byte_to_bcd2_(b_m8563_time_struct->hours)};

  this->write_register(0x02, buf, 3);
}

void BM8563::get_date_(BM8563DateTypeDef *b_m8563_date_struct) {
  uint8_t buf[4] = {0};
  this->read_register(0x05, buf, sizeof(buf));

  b_m8563_date_struct->day = this->bcd2_to_byte_(buf[0] & 0x3f);
  b_m8563_date_struct->week = this->bcd2_to_byte_(buf[1] & 0x07);
  b_m8563_date_struct->month = this->bcd2_to_byte_(buf[2] & 0x1f);

  uint8_t year_byte = this->bcd2_to_byte_(buf[3] & 0xff);
  ESP_LOGD(TAG, "Year byte is %i", year_byte);
  if (buf[2] & 0x80) {
    b_m8563_date_struct->year = 1900 + year_byte;
  } else {
    b_m8563_date_struct->year = 2000 + year_byte;
  }
}

void BM8563::set_date_(BM8563DateTypeDef *b_m8563_date_struct) {
  if (b_m8563_date_struct == NULL) {
    return;
  }
  uint8_t buf[4] = {
      this->byte_to_bcd2_(b_m8563_date_struct->day),
      this->byte_to_bcd2_(b_m8563_date_struct->week),
      this->byte_to_bcd2_(b_m8563_date_struct->month),
      this->byte_to_bcd2_((uint8_t) (b_m8563_date_struct->year % 100)),
  };

  if (b_m8563_date_struct->year < 2000) {
    buf[2] = this->byte_to_bcd2_(b_m8563_date_struct->month) | 0x80;
  } else {
    buf[2] = this->byte_to_bcd2_(b_m8563_date_struct->month) | 0x00;
  }

  ESP_LOGI(TAG, "Writing year is %i", buf[3]);
  this->write_register(0x05, buf, 4);
}

void BM8563::write_reg_(uint8_t reg, uint8_t data) { this->write_byte(reg, data); }

uint8_t BM8563::read_reg_(uint8_t reg) {
  uint8_t data;
  this->read_register(reg, &data, 1);
  return data;
}

int BM8563::set_alarm_irq_(int duration_s) {
  ESP_LOGI(TAG, "Timer Duration: %u s", duration_s);
  uint8_t reg_value = 0;
  reg_value = this->read_reg_(0x01);

  if (duration_s < 0) {
    reg_value &= ~(1 << 0);
    this->write_reg_(0x01, reg_value);
    reg_value = 0x03;
    this->write_reg_(0x0E, reg_value);
    return -1;
  }

  uint8_t type_value = 2;
  uint8_t div = 1;
  if (duration_s > 255) {
    div = 60;
    type_value = 0x83;
  } else {
    type_value = 0x82;
  }

  duration_s = (duration_s / div) & 0xFF;
  this->write_reg_(0x0F, duration_s);
  this->write_reg_(0x0E, type_value);

  reg_value |= (1 << 0);
  reg_value &= ~(1 << 7);
  this->write_reg_(0x01, reg_value);
  return duration_s * div;
}

void BM8563::clear_irq_() {
  uint8_t data = this->read_reg_(0x01);
  this->write_reg_(0x01, data & 0xf3);
}

void BM8563::disable_irq_() {
  this->clear_irq_();
  uint8_t data = this->read_reg_(0x01);
  this->write_reg_(0x01, data & 0xfC);
}

}  // namespace bm8563
}  // namespace esphome
