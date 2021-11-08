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

  void set_oversampling(uint8_t osr) { osr_ = osr; }
  void set_resolution(uint8_t xyz, uint8_t res) { resolutions_[xyz] = res; }
  void set_filter(uint8_t filter) { filter_ = filter; }

  void set_gain(uint8_t gain_sel) { gain_ = gain_sel; }

 protected:
  MLX90393 mlx;
  sensor::Sensor *x_sensor_{nullptr};
  sensor::Sensor *y_sensor_{nullptr};
  sensor::Sensor *z_sensor_{nullptr};
  sensor::Sensor *t_sensor_{nullptr};
  uint8_t gain_ = 0;
  uint8_t osr_ = 0;
  uint8_t filter_ = 0;
  uint8_t resolutions_[3] = {0};
  GPIOPin *drdy_pin_ = nullptr;

};

}  // namespace mlx90393
}  // namespace esphome

