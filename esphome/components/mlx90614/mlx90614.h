#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::mlx90614 {

class MLX90614Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_ambient_sensor(sensor::Sensor *ambient_sensor) { ambient_sensor_ = ambient_sensor; }
  void set_object_sensor(sensor::Sensor *object_sensor) { object_sensor_ = object_sensor; }

  void set_emissivity(float emissivity) { emissivity_ = emissivity; }

 protected:
  i2c::ErrorCode write_emissivity_();

  i2c::ErrorCode write_register_(uint8_t reg, uint16_t data);
  i2c::ErrorCode read_register_(uint8_t reg, uint16_t &data);

  sensor::Sensor *ambient_sensor_{nullptr};
  sensor::Sensor *object_sensor_{nullptr};

  float emissivity_{NAN};
  i2c::ErrorCode emissivity_write_ec_{i2c::ERROR_OK};
  i2c::ErrorCode object_read_ec_{i2c::ERROR_OK};
  i2c::ErrorCode ambient_read_ec_{i2c::ERROR_OK};
};
}  // namespace esphome::mlx90614
