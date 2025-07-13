#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace lps22 {

class LPS22Component : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { this->pressure_sensor_ = pressure_sensor; }

  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};

  RetryResult try_read_();
};

}  // namespace lps22
}  // namespace esphome
