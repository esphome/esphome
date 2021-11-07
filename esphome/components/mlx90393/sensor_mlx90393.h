#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"
#include <MLX90393.h>

namespace esphome {
namespace mlx90393 {

class MLX90393_cls : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_drdy_pin(GPIOPin *pin) { drdy_pin_ = pin; }

  void set_x_sensor(sensor::Sensor *x_sensor) { x_sensor_ = x_sensor; }
  void set_y_sensor(sensor::Sensor *y_sensor) { y_sensor_ = y_sensor; }
  void set_z_sensor(sensor::Sensor *z_sensor) { z_sensor_ = z_sensor; }
  void set_t_sensor(sensor::Sensor *t_sensor) { t_sensor_ = t_sensor; }

  using GAIN = mlx90393_gain;
  void set_gain(GAIN gain) { gain_ = gain; }

 protected:
  MLX90393 mlx;
  sensor::Sensor *x_sensor_{nullptr};
  sensor::Sensor *y_sensor_{nullptr};
  sensor::Sensor *z_sensor_{nullptr};
  sensor::Sensor *t_sensor_{nullptr};
  GAIN gain_ = MLX90393_GAIN_2_5X;
  GPIOPin *drdy_pin_ = nullptr;

};

}  // namespace mlx90393
}  // namespace esphome

