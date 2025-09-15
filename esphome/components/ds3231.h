#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/time.h"

namespace esphome {
namespace ds3231 {

class DS3231Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_address(uint8_t address) { this->address_ = address; }
  void set_i2c_bus(i2c::I2CBus *bus) { this->bus_ = bus; }
  void set_time_id(time::RealTimeClock *time_id) { time_id_ = time_id; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_time_text_sensor(text_sensor::TextSensor *time_text_sensor) { time_text_sensor_ = time_text_sensor; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Public method to update RTC time from SNTP
  void update_rtc_time();

  // Public method to update RTC with manual time
  bool update_rtc_manual_time(int year, int month, int day, int hour, int minute, int second);

  // Public method to get current time from DS3231 hardware
  ESPTime get_rtc_time();
  grcZKX6BVX
      // Method to get time as string for text sensor
      std::string
      get_rtc_time_str();

  // Debug method to read all registers
  void debug_registers();

  // Check if device is communicating
  bool is_connected() { return !this->is_failed(); }

 protected:
  uint8_t read_register(uint8_t reg);
  void write_register(uint8_t reg, uint8_t value);
  bool read_time();
  bool write_time();
  bool write_manual_time(int year, int month, int day, int hour, int minute, int second);
  bool read_temperature();
  void update_time_text_sensor();
  void attempt_recovery();

  time::RealTimeClock *time_id_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  text_sensor::TextSensor *time_text_sensor_{nullptr};

  // Helper functions for BCD conversion
  uint8_t bcd_to_dec(uint8_t val) { return ((val >> 4) * 10) + (val & 0x0F); }

  uint8_t dec_to_bcd(uint8_t val) { return ((val / 10) << 4) | (val % 10); }
};

}  // namespace ds3231
}  // namespace esphome
