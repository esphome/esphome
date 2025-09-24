#pragma once
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/bmp3xx_base/bmp3xx_base.h"

namespace esphome {
namespace bmp3xx_i2c {

class BMP3XXI2CComponent : public bmp3xx_base::BMP3XXComponent, public i2c::I2CDevice {
  bool read_byte(uint8_t a_register, uint8_t *data) override;
  bool write_byte(uint8_t a_register, uint8_t data) override;
  bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
  bool write_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
  void dump_config() override;
};

}  // namespace bmp3xx_i2c
}  // namespace esphome
