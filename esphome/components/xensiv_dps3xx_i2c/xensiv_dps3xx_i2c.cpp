#include "xensiv_dps3xx_i2c.h"

namespace esphome {
namespace xensiv_dps3xx_i2c {

static const char *const TAG = "xensiv_dps3xx_i2c";

void XensivDPS3xxI2C::dump_config() {
  XensivDPS3xx::dump_config();
  ESP_LOGCONFIG(TAG, "Xensiv DPS3xx (I2C):");
  LOG_I2C_DEVICE(this);
}

bool XensivDPS3xxI2C::read_byte(uint8_t reg, uint8_t *data) { return this->read_bytes(reg, data, 1); }

bool XensivDPS3xxI2C::read_bytes(uint8_t reg, uint8_t *data, size_t len) { return this->read(reg, data, len); }

bool XensivDPS3xxI2C::write_byte(uint8_t reg, uint8_t value) { return this->write(&reg, 1, &value, 1); }

}  // namespace xensiv_dps3xx_i2c
}  // namespace esphome
