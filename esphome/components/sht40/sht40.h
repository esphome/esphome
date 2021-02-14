#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace sht40 {

enum SHT40PRECISION {
  SHT40_PRECISION_HIGH = 0,
  SHT40_PRECISION_MED,
  SHT40_PRECISION_LOW
};

enum SHT40HEATERPOWER {
  SHT40_HEATERPOWER_HIGH,
  SHT40_HEATERPOWER_MED,
  SHT40_HEATERPOWER_LOW
};

enum SHT40HEATERTIME {
  SHT40_HEATERTIME_LONG = 1100,
  SHT40_HEATERTIME_SHORT = 110
};


class SHT40Component : public PollingComponent, public i2c::I2CDevice {
 public:
   float get_setup_priority() const override { return setup_priority::DATA; }
   void setup() override;
   void dump_config() override;
   void update() override;

   void set_precision_value(SHT40PRECISION precision) {this->precision_ = precision; };
   void set_heater_power_value(SHT40HEATERPOWER heater_power) {this->heater_power_ = heater_power; };
   void set_heater_time_value(SHT40HEATERTIME heater_time) {this->heater_time_ = heater_time; };
   void set_heater_duty_value(float duty_cycle) {this->duty_cycle_ = duty_cycle; };

   void set_temp_sensor(sensor::Sensor *temp_sensor) { this->temp_sensor_ = temp_sensor; }
   void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }
 protected:
   SHT40PRECISION precision_;
   SHT40HEATERPOWER heater_power_;
   SHT40HEATERTIME heater_time_;
   float duty_cycle_;

   void start_heater_();
   uint8_t heater_command_;


   sensor::Sensor *temp_sensor_{nullptr};
   sensor::Sensor *humidity_sensor_{nullptr};
};

}  // namespace sht40
}  // namespace esphome
