#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ds3231 {

class DS3231Component : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void read_time();
  void write_time();

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }

 protected:
  bool read_rtc_();
  bool write_rtc_();
  uint8_t bcd_to_bin_(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }
  uint8_t bin_to_bcd_(uint8_t bin) { return ((bin / 10) << 4) | (bin % 10); }

  union DS3231Reg {
    struct {
      // 0x00: Seconds
      uint8_t seconds : 4;
      uint8_t seconds_10 : 3;
      uint8_t unused_0 : 1;

      // 0x01: Minutes
      uint8_t minutes : 4;
      uint8_t minutes_10 : 3;
      uint8_t unused_1 : 1;

      // 0x02: Hours
      uint8_t hours : 4;
      uint8_t hours_10 : 2;
      bool is_12h : 1;  // 12-hour mode flag
      bool is_pm : 1;   // PM flag (12-hour mode only)

      // 0x03: Day of week (1-7, 1=Sunday)
      uint8_t day_of_week : 3;
      uint8_t unused_3 : 5;

      // 0x04: Day of month
      uint8_t day : 4;
      uint8_t day_10 : 2;
      uint8_t unused_4 : 2;

      // 0x05: Month + century bit
      uint8_t month : 4;
      uint8_t month_10 : 1;
      bool century : 1;  // Century bit (0=2000-2099, 1=2100-2199)
      uint8_t unused_5 : 2;

      // 0x06: Year
      uint8_t year : 4;
      uint8_t year_10 : 4;

      // 0x07-0x0D: Alarm registers (not used in basic implementation)
      uint8_t alarm1_seconds;
      uint8_t alarm1_minutes;
      uint8_t alarm1_hours;
      uint8_t alarm1_day_date;
      uint8_t alarm2_minutes;
      uint8_t alarm2_hours;
      uint8_t alarm2_day_date;

      // 0x0E: Control register
      uint8_t rs : 2;  // Rate select
      uint8_t unused_e1 : 2;
      bool intcn : 1;  // Interrupt control
      bool a2ie : 1;   // Alarm 2 interrupt enable
      bool a1ie : 1;   // Alarm 1 interrupt enable
      bool eosc : 1;   // Enable oscillator

      // 0x0F: Status register
      uint8_t unused_f0 : 2;
      bool en32khz : 1;  // Enable 32kHz output
      bool bsy : 1;      // Busy
      bool a2f : 1;      // Alarm 2 flag
      bool a1f : 1;      // Alarm 1 flag
      bool osf : 1;      // Oscillator stop flag

      // 0x10: Aging offset
      uint8_t aging_offset;

      // 0x11-0x12: Temperature (MSB + LSB)
      uint8_t temp_msb;
      uint8_t temp_lsb : 6;
      uint8_t unused_12 : 2;
    } reg;
    mutable uint8_t raw[19];
  } ds3231_;

  sensor::Sensor *temperature_sensor_{nullptr};
};

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  void play(Ts... x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<DS3231Component> {
 public:
  void play(Ts... x) override { this->parent_->read_time(); }
};

}  // namespace ds3231
}  // namespace esphome
