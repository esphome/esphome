#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"

namespace esphome {
namespace ds3231 {

class DS3231SensorComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void update() override;

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
};

}  // namespace ds3231
}  // namespace esphome
