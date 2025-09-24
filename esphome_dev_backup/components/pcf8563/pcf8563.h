#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace pcf8563 {

class PCF8563Component : public time::RealTimeClock, public i2c::I2CDevice {
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
  union PCF8563Reg {
    struct {
      // Control_1 register
      bool : 3;
      bool power_on_reset : 1;
      bool : 1;
      bool stop : 1;
      bool : 1;
      bool ext_test : 1;

      // Control_2 register
      bool time_int : 1;
      bool alarm_int : 1;
      bool timer_flag : 1;
      bool alarm_flag : 1;
      bool timer_int_timer_pulse : 1;
      bool : 3;

      // Seconds register
      uint8_t second : 4;
      uint8_t second_10 : 3;
      bool clock_int : 1;

      // Minutes register
      uint8_t minute : 4;
      uint8_t minute_10 : 3;
      uint8_t : 1;

      // Hours register
      uint8_t hour : 4;
      uint8_t hour_10 : 2;
      uint8_t : 2;

      // Days register
      uint8_t day : 4;
      uint8_t day_10 : 2;
      uint8_t : 2;

      // Weekdays register
      uint8_t weekday : 3;
      uint8_t unused_3 : 5;

      // Months register
      uint8_t month : 4;
      uint8_t month_10 : 1;
      uint8_t : 2;
      uint8_t century : 1;

      // Years register
      uint8_t year : 4;
      uint8_t year_10 : 4;

      // Minute Alarm register
      uint8_t minute_alarm : 4;
      uint8_t minute_alarm_10 : 3;
      bool minute_alarm_enabled : 1;

      // Hour Alarm register
      uint8_t hour_alarm : 4;
      uint8_t hour_alarm_10 : 2;
      uint8_t : 1;
      bool hour_alarm_enabled : 1;

      // Day Alarm register
      uint8_t day_alarm : 4;
      uint8_t day_alarm_10 : 2;
      uint8_t : 1;
      bool day_alarm_enabled : 1;

      // Weekday Alarm register
      uint8_t weekday_alarm : 3;
      uint8_t : 4;
      bool weekday_alarm_enabled : 1;

      // CLKout control register
      uint8_t frequency_output : 2;
      uint8_t : 5;
      uint8_t clkout_enabled : 1;

      // Timer control register
      uint8_t timer_source_frequency : 2;
      uint8_t : 5;
      uint8_t timer_enabled : 1;

      // Timer register
      uint8_t countdown_period : 8;

    } reg;
    mutable uint8_t raw[sizeof(reg)];
  } pcf8563_;
};

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<PCF8563Component> {
 public:
  void play(Ts... x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<PCF8563Component> {
 public:
  void play(Ts... x) override { this->parent_->read_time(); }
};
}  // namespace pcf8563
}  // namespace esphome
