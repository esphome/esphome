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
  
  // DS3231 register structure
  struct DS3231Registers {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day_of_week;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t alarm1_seconds;
    uint8_t alarm1_minutes;
    uint8_t alarm1_hours;
    uint8_t alarm1_day_date;
    uint8_t alarm2_minutes;
    uint8_t alarm2_hours;
    uint8_t alarm2_day_date;
    uint8_t control;
    uint8_t status;
    uint8_t aging_offset;
    uint8_t temp_msb;
    uint8_t temp_lsb;
  } ds3231_reg_{};
  
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
