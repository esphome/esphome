#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace xdb401 {

/// This class implements support for the xdb401 pressure and temperature i2c sensors.
class XDB401Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_raw_pressure_sensor(sensor::Sensor *pressure_sensor) { this->pressure_sensor_ = pressure_sensor; }

  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  i2c::ErrorCode read_(float &temperature, float &pressure);
  i2c::ErrorCode set_meas_mode_();
  i2c::ErrorCode read_pressure_(float &pressure);
  i2c::ErrorCode read_temperature_(float &temperature);
  uint8_t comm_err_counter_{0};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
};

}  // namespace xdb401
}  // namespace esphome
