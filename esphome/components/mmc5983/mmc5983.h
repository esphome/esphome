#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mmc5983 {

class MMC5983Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void update() override;
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_x_sensor(sensor::Sensor *x_sensor) { x_sensor_ = x_sensor; }
  void set_y_sensor(sensor::Sensor *y_sensor) { y_sensor_ = y_sensor; }
  void set_z_sensor(sensor::Sensor *z_sensor) { z_sensor_ = z_sensor; }

 protected:
  sensor::Sensor *x_sensor_{nullptr};
  sensor::Sensor *y_sensor_{nullptr};
  sensor::Sensor *z_sensor_{nullptr};
};

}  // namespace mmc5983
}  // namespace esphome
