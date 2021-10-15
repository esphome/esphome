#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace ds3231 {

enum DS3231Alarm1Mode {
  EVERY_SECOND = 0x0F,
  MATCH_SECOND = 0x0E,
  MATCH_MINUTE_SECOND = 0x0C,
  MATCH_HOUR_MINUTE_SECOND = 0x08,
  MATCH_DAY_OF_MONTH_HOUR_MINUTE_SECOND = 0x00,
  MATCH_DAY_OF_WEEK_HOUR_MINUTE_SECOND = 0x10,
};

enum DS3231Alarm2Mode {
  EVERY_MINUTE = 0x0E,
  MATCH_MINUTE = 0x0C,
  MATCH_HOUR_MINUTE = 0x08,
  MATCH_DAY_OF_MONTH_HOUR_MINUTE = 0x00,
  MATCH_DAY_OF_WEEK_HOUR_MINUTE = 0x10,
};

enum DS3231SquareWaveMode {
  MODE_SQUARE_WAVE,
  MODE_ALARM_INTERRUPT,
};

enum DS3231SquareWaveFrequency {
  FREQUENCY_1_HZ = 0x00,
  FREQUENCY_1024_HZ = 0x01,
  FREQUENCY_4096_HZ = 0x02,
  FREQUENCY_8192_HZ = 0x03,
};

class DS3231RTC;

class DS3231Sensor;

class DS3231Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_default_alarm_1(DS3231Alarm1Mode mode, bool int_enabled, uint8_t second, uint8_t minute, uint8_t hour,
                           uint8_t day);
  void set_default_alarm_2(DS3231Alarm2Mode mode, bool int_enabled, uint8_t minute, uint8_t hour, uint8_t day);
  void set_default_square_wave_mode(DS3231SquareWaveMode mode) { this->square_wave_mode_ = mode; }
  void set_default_square_wave_frequency(DS3231SquareWaveFrequency frequency) {
    this->square_wave_frequency_ = frequency;
  }
  void add_on_alarm_callback(std::function<void(uint8_t)> &&callback);
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override { this->read_status_(); }

  void set_alarm_1(DS3231Alarm1Mode mode, bool int_enabled, uint8_t second, uint8_t minute, uint8_t hour, uint8_t day);
  void reset_alarm_1();
  void set_alarm_2(DS3231Alarm2Mode mode, bool int_enabled, uint8_t minute, uint8_t hour, uint8_t day);
  void reset_alarm_2();
  void set_square_wave_mode(DS3231SquareWaveMode mode);
  void set_square_wave_frequency(DS3231SquareWaveFrequency frequency);

 protected:
  friend DS3231RTC;
  friend DS3231Sensor;

  bool read_rtc_();
  bool write_rtc_();
  bool read_alarm_();
  bool write_alarm_();
  bool read_control_();
  bool write_control_();
  bool read_status_(bool initial_read = false);
  bool write_status_();
  bool read_temperature_();
  CallbackManager<void(uint8_t)> alarm_callback_{};

  optional<DS3231Alarm1Mode> alarm_1_mode_{};
  bool alarm_1_interrupt_anabled;
  uint8_t alarm_1_second_;
  uint8_t alarm_1_minute_;
  uint8_t alarm_1_hour_;
  uint8_t alarm_1_day_;
  optional<DS3231Alarm2Mode> alarm_2_mode_{};
  bool alarm_2_interrupt_anabled;
  uint8_t alarm_2_minute_;
  uint8_t alarm_2_hour_;
  uint8_t alarm_2_day_;
  optional<DS3231SquareWaveMode> square_wave_mode_;
  optional<DS3231SquareWaveFrequency> square_wave_frequency_;
  bool alarm_1_act_;
  bool alarm_2_act_;
  struct DS3231Reg {
    union DS3231RTC {
      struct {
        uint8_t second : 4;
        uint8_t second_10 : 3;
        uint8_t unused_1 : 1;

        uint8_t minute : 4;
        uint8_t minute_10 : 3;
        uint8_t unused_2 : 1;

        uint8_t hour : 4;
        uint8_t hour_10 : 2;
        uint8_t unused_3 : 2;

        uint8_t weekday : 3;
        uint8_t unused_4 : 5;

        uint8_t day : 4;
        uint8_t day_10 : 2;
        uint8_t unused_5 : 2;

        uint8_t month : 4;
        uint8_t month_10 : 1;
        uint8_t unused_6 : 2;
        uint8_t cent : 1;

        uint8_t year : 4;
        uint8_t year_10 : 4;
      } reg;
      mutable uint8_t raw[sizeof(reg)];
    } rtc;
    union DS3231Alrm {
      struct {
        uint8_t a1_second : 4;
        uint8_t a1_second_10 : 3;
        bool a1_m1 : 1;

        uint8_t a1_minute : 4;
        uint8_t a1_minute_10 : 3;
        bool a1_m2 : 1;

        uint8_t a1_hour : 4;
        uint8_t a1_hour_10 : 2;
        uint8_t unused_1 : 1;
        bool a1_m3 : 1;

        uint8_t a1_day : 4;
        uint8_t a1_day_10 : 2;
        uint8_t a1_day_mode : 1;
        bool a1_m4 : 1;

        uint8_t a2_minute : 4;
        uint8_t a2_minute_10 : 3;
        bool a2_m2 : 1;

        uint8_t a2_hour : 4;
        uint8_t a2_hour_10 : 2;
        uint8_t unused_2 : 1;
        bool a2_m3 : 1;

        uint8_t a2_day : 4;
        uint8_t a2_day_10 : 2;
        uint8_t a2_day_mode : 1;
        bool a2_m4 : 1;
      } reg;
      mutable uint8_t raw[sizeof(reg)];
    } alrm;
    union DS3231Ctrl {
      struct {
        bool alrm_1_int : 1;
        bool alrm_2_int : 1;
        bool int_ctrl : 1;
        uint8_t rs : 2;
        bool conv_tmp : 1;
        bool bat_sqw : 1;
        bool osc_dis : 1;
      } reg;
      mutable uint8_t raw[sizeof(reg)];
    } ctrl;
    union DS3231Stat {
      struct {
        bool alrm_1_act : 1;
        bool alrm_2_act : 1;
        bool busy : 1;
        bool en32khz : 1;
        uint8_t unused : 3;
        bool osc_stop : 1;
      } reg;
      mutable uint8_t raw[sizeof(reg)];
    } stat;
    union DS3231Temp {
      struct {
        int8_t upper : 8;
        uint8_t lower : 2;
        uint8_t unused : 6;
      } reg;
      mutable uint8_t raw[sizeof(reg)];
    } temp;
  } ds3231_;
};

class DS3231RTC : public time::RealTimeClock, public Parented<DS3231Component> {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;
  void update() override { this->read_time(); }
  void read_time();
  void write_time();
};

class DS3231Sensor : public PollingComponent, public sensor::Sensor, public Parented<DS3231Component> {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;
  void update() override;
};

}  // namespace ds3231
}  // namespace esphome
