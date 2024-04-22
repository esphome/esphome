#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "AMS5915.h"

namespace esphome {
namespace ams5915 {

class Ams5915 : public PollingComponent, public sensor::Sensor, public i2c::I2CDevice {
 public:
  Ams5915(): PollingComponent(5000) {}
  void dump_config() override;
  void setup() override;
  void update() override;
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }

 protected:
    sensor::Sensor *temperature_sensor_{nullptr};
    sensor::Sensor *pressure_sensor_{nullptr};
};

}  // namespace ams5915
}  // namespace esphome