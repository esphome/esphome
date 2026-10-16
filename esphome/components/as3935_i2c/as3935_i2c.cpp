#include "as3935_i2c.h"
#include "esphome/core/log.h"

namespace esphome::as3935_i2c {

ESPHOME_LOG_TAG(TAG, "as3935_i2c");

void I2CAS3935Component::write_register(uint8_t reg, uint8_t mask, uint8_t bits, uint8_t start_pos) {
  const uint8_t value = bits << start_pos;
  // Bits of value outside mask were ORed in before, so they stay part of the update
  if (!this->update_register(reg, mask | value, value)) {
    this->mark_failed();
    ESP_LOGW(TAG, "Failed to update register 0x%02X", reg);
  }
}

uint8_t I2CAS3935Component::read_register(uint8_t reg) {
  uint8_t value;
  if (!this->read_byte(reg, &value)) {
    ESP_LOGW(TAG, "Reading register failed!");
    return 0;
  }
  return value;
}
void I2CAS3935Component::dump_config() {
  AS3935Component::dump_config();
  LOG_I2C_DEVICE(this);
}

}  // namespace esphome::as3935_i2c
