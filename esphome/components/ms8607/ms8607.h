#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ms8607 {

class MS8607Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; };

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  /// Creates `humidity_i2c_device_` using the provided `address`
  void set_humidity_sensor_address(uint8_t address);

 protected:
  /// Reset both I2CDevices, and return true if this was successful
  bool reset_();
  /// Read and store the Pressure & Temperature calibration settings from the PROM
  bool read_calibration_values_from_prom_();

  void read_temperature_();
  void read_pressure_(uint32_t raw_temperature);
  void calculate_values_(uint32_t raw_temperature, uint32_t raw_pressure);

  sensor::Sensor *temperature_sensor_;
  sensor::Sensor *pressure_sensor_;
  sensor::Sensor *humidity_sensor_;

  /** Secondary I2C address for the humidity sensor, and an I2CDevice object to communicate with it.
   *
   * Uses the same I2C bus & settings as `this` object, since the MS8607 only has one set of pins. This
   * I2CDevice object is an implementation detail of the Component. The esphome configuration only
   * cares what the I2C address of the humidity sensor is. (Default is 0x40)
   */
  i2c::I2CDevice *humidity_i2c_device_;

  uint16_t prom_[7];
};

}  // namespace ms8607
}  // namespace esphome
