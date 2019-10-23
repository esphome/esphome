#include "as3935_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace as3935_i2c {

static const char *TAG = "as3935_i2c";

void I2CAS3935Component::write_register(uint8_t reg, uint8_t mask, uint8_t bits, uint8_t start_pos) {
  uint8_t write_reg;
  if (!this->read_byte(reg, &write_reg)) {
    this->mark_failed();
    ESP_LOGW(TAG, "read_byte failed - increase log level for more details!");
    return;
  }

  write_reg &= (~mask);
  write_reg |= (bits << start_pos);

  if (!this->write_byte(reg, write_reg)) {
    ESP_LOGW(TAG, "write_byte failed - increase log level for more details!");
    return;
  }
}

uint8_t I2CAS3935Component::read_register(uint8_t reg) {
  uint8_t value;
  if (!this->read_byte(reg, &value, 2)) {
    ESP_LOGW(TAG, "Read failed!");
    return 0;
  }
  return value;
}
void I2CAS3935Component::dump_config() {
  AS3935Component::dump_config();
  LOG_I2C_DEVICE(this);
}

}  // namespace as3935_i2c
}  // namespace esphome
