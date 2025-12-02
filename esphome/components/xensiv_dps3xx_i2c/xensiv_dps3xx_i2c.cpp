#include "esphome/core/log.h"
#include "xensiv_dps3xx_i2c.h"

namespace esphome {
namespace xensiv_dps3xx_i2c {
static const char *const TAG = "xensiv_dps3xx_i2c.component";

void XensivDPS3xxI2C::dump_config() {
  LOG_I2C_DEVICE(this);
  XensivDPS3xx::dump_config();
}

bool XensivDPS3xxI2C::read_byte(uint8_t reg, uint8_t *data) { return I2CDevice::read_byte(reg, data); }

bool XensivDPS3xxI2C::read_bytes(uint8_t reg, uint8_t *data, size_t len) {
  return I2CDevice::read_bytes(reg, data, len);
}

bool XensivDPS3xxI2C::write_byte(uint8_t reg, uint8_t value) { return I2CDevice::write_byte(reg, value); }
}  // namespace xensiv_dps3xx_i2c
}  // namespace esphome
