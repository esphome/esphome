#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

#include <Wire.h>
#include "bme680.h"

namespace esphome {
namespace bme680 {

#define BME680_I2C_ADDR 0x77

class BME680Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature) { this->temperature_sensor_ = temperature; }
  void set_pressure_sensor(sensor::Sensor *pressure) { this->pressure_sensor_ = pressure; }
  void set_humidity_sensor(sensor::Sensor *humidity) { this->humidity_sensor_ = humidity; }
  void set_gas_resistance_sensor(sensor::Sensor *gas) { this->gas_resistance_sensor_ = gas; }
  
  void turn_off_heater() { this->heater_off_ = true; }
  bool is_heater_off() { return this->heater_off_; }

  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *gas_resistance_sensor_{nullptr};
  
  bool heater_off_{false};
  struct bme680_dev dev;
  TwoWire *wire{nullptr};
};

}  // namespace bme680
}  // namespace esphome
