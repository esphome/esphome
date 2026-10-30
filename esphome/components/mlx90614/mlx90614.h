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

  SUB_SENSOR(ambient)
  SUB_SENSOR(object)

  void set_emissivity(float emissivity) { emissivity_ = emissivity; }

 protected:
  void try_write_emissivity_();
  bool write_emissivity_();

  bool write_register_(uint8_t reg, uint16_t data);
  i2c::ErrorCode read_register_(uint8_t reg, uint16_t &data);

  float emissivity_{NAN};
  // Remaining attempts to program the emissivity EEPROM cell, bounded to limit cell wear
  uint8_t emissivity_write_attempts_{0};
  bool emissivity_write_failed_{false};
};
}  // namespace esphome::mlx90614
