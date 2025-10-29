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
  if (this->timer_value_.has_value()) {
    uint32_t timer_value = *this->timer_value_;
    ESP_LOGCONFIG(TAG, "  Timer Value: %u s", timer_value);
  }
}

void BM8563::set_timer_value(uint32_t timer_s) {
  ESP_LOGI(TAG, "Timer Value Setting to: %u s", timer_s);
  this->timer_value_ = timer_s;
}

void BM8563::start_timer(uint32_t timer_s) {
  if (this->setup_complete_) {
    this->clear_irq();
    this->set_alarm_irq(timer_s);
  }
}

void BM8563::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }

  BM8563_TimeTypeDef BM8563_TimeStruct{
      .hours = int8_t(now.hour),
      .minutes = int8_t(now.minute),
      .seconds = int8_t(now.second),
  };

  BM8563_DateTypeDef BM8563_DateStruct{.day = int8_t(now.day_of_month),
                                       .week = int8_t(now.day_of_week),
                                       .month = int8_t(now.month),
                                       .year = int16_t(now.year)};

  this->set_time(&BM8563_TimeStruct);
  this->set_date(&BM8563_DateStruct);
}

void BM8563::read_time() {
  BM8563_TimeTypeDef BM8563_TimeStruct;
  BM8563_DateTypeDef BM8563_DateStruct;
  this->get_time(&BM8563_TimeStruct);
  this->get_date(&BM8563_DateStruct);
  ESP_LOGD(TAG, "BM8563: %i-%i-%i %i, %i:%i:%i", BM8563_DateStruct.year, BM8563_DateStruct.month, BM8563_DateStruct.day,
           BM8563_DateStruct.week, BM8563_TimeStruct.hours, BM8563_TimeStruct.minutes, BM8563_TimeStruct.seconds);

  ESPTime rtc_time{
      .second = uint8_t(BM8563_TimeStruct.seconds),
      .minute = uint8_t(BM8563_TimeStruct.minutes),
      .hour = uint8_t(BM8563_TimeStruct.hours),
      .day_of_week = uint8_t(BM8563_DateStruct.week),
      .day_of_month = uint8_t(BM8563_DateStruct.day),
      .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
      .month = uint8_t(BM8563_DateStruct.month),
      .year = uint16_t(BM8563_DateStruct.year),
      .is_dst = false,  // ignored by recalc_timestamp_utc()
      .timestamp = 0    // result
  };
  rtc_time.recalc_timestamp_utc(false);
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

bool BM8563::get_volt_low() {
  uint8_t data = this->read_reg(0x02);
  return data & 0x80;  // RTCC_VLSEC_MASK
}

uint8_t BM8563::bcd2_to_byte(uint8_t value) {
  uint8_t tmp = 0;
  tmp = ((uint8_t) (value & (uint8_t) 0xF0) >> (uint8_t) 0x4) * 10;
  return (tmp + (value & (uint8_t) 0x0F));
}

uint8_t BM8563::byte_to_bcd2(uint8_t value) {
  uint8_t bcdhigh = 0;

  while (value >= 10) {
    bcdhigh++;
    value -= 10;
  }

  return ((uint8_t) (bcdhigh << 4) | value);
}

void BM8563::get_time(BM8563_TimeTypeDef *BM8563_TimeStruct) {
  uint8_t buf[3] = {0};

  this->read_register(0x02, buf, 3);

  BM8563_TimeStruct->seconds = this->bcd2_to_byte(buf[0] & 0x7f);
  BM8563_TimeStruct->minutes = this->bcd2_to_byte(buf[1] & 0x7f);
  BM8563_TimeStruct->hours = this->bcd2_to_byte(buf[2] & 0x3f);
}

void BM8563::set_time(BM8563_TimeTypeDef *BM8563_TimeStruct) {
  if (BM8563_TimeStruct == NULL) {
    return;
  }
  uint8_t buf[3] = {this->byte_to_bcd2(BM8563_TimeStruct->seconds), this->byte_to_bcd2(BM8563_TimeStruct->minutes),
                    this->byte_to_bcd2(BM8563_TimeStruct->hours)};

  this->write_register(0x02, buf, 3);
}

void BM8563::get_date(BM8563_DateTypeDef *BM8563_DateStruct) {
  uint8_t buf[4] = {0};
  this->read_register(0x05, buf, sizeof(buf));

  BM8563_DateStruct->day = this->bcd2_to_byte(buf[0] & 0x3f);
  BM8563_DateStruct->week = this->bcd2_to_byte(buf[1] & 0x07);
  BM8563_DateStruct->month = this->bcd2_to_byte(buf[2] & 0x1f);

  uint8_t year_byte = this->bcd2_to_byte(buf[3] & 0xff);
  ESP_LOGD(TAG, "Year byte is %i", year_byte);
  if (buf[2] & 0x80) {
    BM8563_DateStruct->year = 1900 + year_byte;
  } else {
    BM8563_DateStruct->year = 2000 + year_byte;
  }
}

void BM8563::set_date(BM8563_DateTypeDef *BM8563_DateStruct) {
  if (BM8563_DateStruct == NULL) {
    return;
  }
  uint8_t buf[4] = {
      this->byte_to_bcd2(BM8563_DateStruct->day),
      this->byte_to_bcd2(BM8563_DateStruct->week),
      this->byte_to_bcd2(BM8563_DateStruct->month),
      this->byte_to_bcd2((uint8_t) (BM8563_DateStruct->year % 100)),
  };

  if (BM8563_DateStruct->year < 2000) {
    buf[2] = this->byte_to_bcd2(BM8563_DateStruct->month) | 0x80;
  } else {
    buf[2] = this->byte_to_bcd2(BM8563_DateStruct->month) | 0x00;
  }

  ESP_LOGI(TAG, "Writing year is %i", buf[3]);
  this->write_register(0x05, buf, 4);
}

void BM8563::write_reg(uint8_t reg, uint8_t data) { this->write_byte(reg, data); }

uint8_t BM8563::read_reg(uint8_t reg) {
  uint8_t data;
  this->read_register(reg, &data, 1);
  return data;
}

int BM8563::set_alarm_irq(int duration_s) {
  ESP_LOGI(TAG, "Timer Duration: %u s", duration_s);
  uint8_t reg_value = 0;
  reg_value = this->read_reg(0x01);

  if (duration_s < 0) {
    reg_value &= ~(1 << 0);
    this->write_reg(0x01, reg_value);
    reg_value = 0x03;
    this->write_reg(0x0E, reg_value);
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
  this->write_reg(0x0F, duration_s);
  this->write_reg(0x0E, type_value);

  reg_value |= (1 << 0);
  reg_value &= ~(1 << 7);
  this->write_reg(0x01, reg_value);
  return duration_s * div;
}

void BM8563::clear_irq() {
  uint8_t data = this->read_reg(0x01);
  this->write_reg(0x01, data & 0xf3);
}

void BM8563::disable_irq() {
  this->clear_irq();
  uint8_t data = this->read_reg(0x01);
  this->write_reg(0x01, data & 0xfC);
}

}  // namespace bm8563
}  // namespace esphome
