#include "esphome/core/log.h"
#include "xensiv_pasco2_i2c.h"

namespace esphome {
namespace xensiv_pasco2_i2c {
static const char *const TAG = "xensiv_pasco2_i2c.component";

void XensivPasCO2I2CComponent::dump_config() {
  LOG_I2C_DEVICE(this);
  XensivPasCO2::dump_config();
}

bool XensivPasCO2I2CComponent::read_byte(uint8_t reg, uint8_t *data) { return I2CDevice::read_byte(reg, data); }

bool XensivPasCO2I2CComponent::read_bytes(uint8_t reg, uint8_t *data, size_t len) {
  return I2CDevice::read_bytes(reg, data, len);
}

bool XensivPasCO2I2CComponent::write_byte(uint8_t reg, uint8_t value) { return I2CDevice::write_byte(reg, value); }
}  // namespace xensiv_pasco2_i2c
}  // namespace esphome
