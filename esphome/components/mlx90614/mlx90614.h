#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mlx90614 {

class MLX90614Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_target_temperature(sensor::Sensor *temperature) { temperature_target_ = temperature; }
  void set_reference_temperature(sensor::Sensor *temperature) { temperature_reference_ = temperature; }

  /// Setup (reset) the sensor and check connection.
  void setup() override;
  void dump_config() override;
  /// Update the sensor values (temperature+humidity).
  void update() override;

  float get_setup_priority() const { return setup_priority::DATA; }

 protected:
  float read_temp_register_(uint8_t reg);

  sensor::Sensor *temperature_target_{nullptr};
  sensor::Sensor *temperature_reference_{nullptr};
};

}  // namespace mlx90614
}  // namespace esphome
