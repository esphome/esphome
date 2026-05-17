#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/lis3dh/lis3dh.h"

namespace esphome {
namespace lis3dh_i2c {

class LIS3DHI2C : public lis3dh::LIS3DHComponent, public i2c::I2CDevice {
 public:
  void dump_config() override;

  bool read_register(uint8_t reg, uint8_t *data, uint16_t len) override;
  bool write_register(uint8_t reg, const uint8_t *data, uint16_t len) override;
};

}  // namespace lis3dh_i2c
}  // namespace esphome
