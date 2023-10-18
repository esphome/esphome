#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace xgzp68xx {

class XGZP68XXComponent : public PollingComponent, public sensor::Sensor, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }
  
  void update() override;
  void setup() override;
  void dump_config() override;

 protected:
  /// Internal method to read the pressure from the component after it has been scheduled.
  void read_pressure_();
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
};

}  // namespace xgzp68xx
}  // namespace esphome
