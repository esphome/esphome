#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace mlx90614 {

class MLX90614Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void set_ambient_sensor(sensor::Sensor *ambient_sensor) { ambient_sensor_ = ambient_sensor; }
  void set_object_sensor(sensor::Sensor *object_sensor) { object_sensor_ = object_sensor; }

  void set_emissivity(float emissivity) { emissivity_ = emissivity; }

 protected:
  bool write_emissivity_();

  uint8_t crc8_pec_(const uint8_t *data, uint8_t len);
  bool write_bytes_(uint8_t reg, uint16_t data);

  sensor::Sensor *ambient_sensor_{nullptr};
  sensor::Sensor *object_sensor_{nullptr};

  float emissivity_{NAN};
};
}  // namespace mlx90614
}  // namespace esphome
