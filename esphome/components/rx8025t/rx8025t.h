#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome::rx8025t {

class RX8025TComponent : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void read_time();
  void write_time();

 protected:
  bool read_rtc_();
  bool write_rtc_();

  // Register addresses
  static const uint8_t REG_SECONDS = 0x00;
  static const uint8_t REG_MINUTES = 0x01;
  static const uint8_t REG_HOURS = 0x02;
  static const uint8_t REG_WEEKDAY = 0x03;
  static const uint8_t REG_DAY = 0x04;
  static const uint8_t REG_MONTH = 0x05;
  static const uint8_t REG_YEAR = 0x06;
  static const uint8_t REG_RAM = 0x07;
  static const uint8_t REG_ALARM_MIN = 0x08;
  static const uint8_t REG_ALARM_HOUR = 0x09;
  static const uint8_t REG_ALARM_DAY = 0x0A;
  static const uint8_t REG_TIMER0 = 0x0B;
  static const uint8_t REG_TIMER1 = 0x0C;
  static const uint8_t REG_EXTENSION = 0x0D;
  static const uint8_t REG_FLAG = 0x0E;
  static const uint8_t REG_CONTROL = 0x0F;

  // Flag register bits
  static const uint8_t FLAG_VDET = 0x01;  // Voltage detection flag
  static const uint8_t FLAG_VLF = 0x02;   // Voltage low flag (oscillator stopped)

  union RX8025TReg {
    struct {
      // 0x00 - Seconds (BCD)
      uint8_t second : 4;
      uint8_t second_10 : 3;
      uint8_t unused_0 : 1;

      // 0x01 - Minutes (BCD)
      uint8_t minute : 4;
      uint8_t minute_10 : 3;
      uint8_t unused_1 : 1;

      // 0x02 - Hours (BCD, 24h format)
      uint8_t hour : 4;
      uint8_t hour_10 : 2;
      uint8_t unused_2 : 2;

      // 0x03 - Weekday (0-6, Sunday = 0)
      uint8_t weekday : 3;
      uint8_t unused_3 : 5;

      // 0x04 - Day (BCD)
      uint8_t day : 4;
      uint8_t day_10 : 2;
      uint8_t unused_4 : 2;

      // 0x05 - Month (BCD)
      uint8_t month : 4;
      uint8_t month_10 : 1;
      uint8_t unused_5 : 3;

      // 0x06 - Year (BCD, 00-99)
      uint8_t year : 4;
      uint8_t year_10 : 4;

      // 0x07 - RAM
      uint8_t ram;

      // 0x08 - Alarm Minutes
      uint8_t alarm_minute : 4;
      uint8_t alarm_minute_10 : 3;
      bool alarm_minute_enable : 1;

      // 0x09 - Alarm Hours
      uint8_t alarm_hour : 4;
      uint8_t alarm_hour_10 : 2;
      uint8_t unused_9 : 1;
      bool alarm_hour_enable : 1;

      // 0x0A - Alarm Day/Weekday
      uint8_t alarm_day : 4;
      uint8_t alarm_day_10 : 2;
      uint8_t unused_a : 1;
      bool alarm_day_enable : 1;

      // 0x0B - Timer Counter 0
      uint8_t timer0;

      // 0x0C - Timer Counter 1
      uint8_t timer1 : 4;
      uint8_t unused_c : 4;

      // 0x0D - Extension Register
      uint8_t tsel : 2;  // Timer clock select
      uint8_t fsel : 2;  // FOUT frequency select
      bool te : 1;       // Timer enable
      bool usel : 1;     // Update interrupt select
      bool wada : 1;     // Weekday/Day alarm select
      uint8_t unused_d : 1;

      // 0x0E - Flag Register
      bool vdet : 1;  // Voltage detection flag
      bool vlf : 1;   // Voltage low flag (oscillator stopped)
      uint8_t unused_e1 : 1;
      bool af : 1;  // Alarm flag
      bool tf : 1;  // Timer flag
      bool uf : 1;  // Update flag
      uint8_t unused_e2 : 2;

      // 0x0F - Control Register
      bool reset : 1;  // Reset bit
      uint8_t unused_f1 : 2;
      bool aie : 1;      // Alarm interrupt enable
      bool tie : 1;      // Timer interrupt enable
      bool uie : 1;      // Update interrupt enable
      uint8_t csel : 2;  // Compensation interval select
    } reg;
    mutable uint8_t raw[sizeof(reg)];
  } rx8025t_;
};

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<RX8025TComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<RX8025TComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->read_time(); }
};

}  // namespace esphome::rx8025t
