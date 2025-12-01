#pragma once

#include "esphome/components/xensiv_dps3xx_base/xensiv_dps3xx_base.h"
#include "esphome/components/i2c/i2c.h"
namespace esphome {
namespace xensiv_dps3xx_i2c {

class XensivDPS3xxI2C : public esphome::xensiv_dps3xx_base::XensivDPS3xx, public i2c::I2CDevice {
 public:
 protected:
  void dump_config() override;
  bool read_byte(uint8_t reg, uint8_t *data) override;
  bool read_bytes(uint8_t reg, uint8_t *data, size_t len) override;
  bool write_byte(uint8_t reg, uint8_t value) override;
};

}  // namespace xensiv_dps3xx_i2c
}  // namespace esphome
