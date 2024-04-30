#include "ina2xx_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ina2xx_i2c {

static const char *const TAG = "ina2xx_i2c";

void INA2XXI2C::setup() {
  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
  INA2XX::setup();
}

void INA2XXI2C::dump_config() {
  INA2XX::dump_config();
  LOG_I2C_DEVICE(this);
}

bool INA2XXI2C::read_ina_register(uint8_t reg, uint8_t *data, size_t len) {
  auto ret = this->read_register(reg, data, len, false);
  if (ret != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "read_ina_register_ failed. Reg=0x%02X Err=%d", reg, ret);
  }
  return ret == i2c::ERROR_OK;
}

bool INA2XXI2C::write_ina_register(uint8_t reg, const uint8_t *data, size_t len) {
  auto ret = this->write_register(reg, data, len);
  if (ret != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "write_register failed. Reg=0x%02X Err=%d", reg, ret);
  }
  return ret == i2c::ERROR_OK;
}
}  // namespace ina2xx_i2c
}  // namespace esphome
