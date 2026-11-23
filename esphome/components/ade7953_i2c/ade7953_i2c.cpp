#include "ade7953_i2c.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ade7953_i2c {

ESPHOME_LOG_TAG(TAG, "ade7953");

void AdE7953I2c::dump_config() {
  ESP_LOGCONFIG(TAG, "ADE7953_i2c:");
  LOG_I2C_DEVICE(this);
  ade7953_base::ADE7953::dump_config();
}
bool AdE7953I2c::ade_write_8(uint16_t reg, uint8_t value) { return !this->write_byte_reg16(reg, value); }
bool AdE7953I2c::ade_write_16(uint16_t reg, uint16_t value) {
  uint8_t data[4];
  put_be16(data, reg);
  put_be16(data + 2, value);
  return this->write(data, 4) != i2c::ERROR_OK;
}
bool AdE7953I2c::ade_write_32(uint16_t reg, uint32_t value) {
  uint8_t data[6];
  put_be16(data, reg);
  put_be32(data + 2, value);
  return this->write(data, 6) != i2c::ERROR_OK;
}
bool AdE7953I2c::ade_read_8(uint16_t reg, uint8_t *value) {
  uint8_t reg_data[2];
  put_be16(reg_data, reg);
  i2c::ErrorCode err = this->write(reg_data, 2);
  if (err != i2c::ERROR_OK)
    return true;
  err = this->read(value, 1);
  return (err != i2c::ERROR_OK);
}
bool AdE7953I2c::ade_read_16(uint16_t reg, uint16_t *value) {
  uint8_t reg_data[2];
  put_be16(reg_data, reg);
  i2c::ErrorCode err = this->write(reg_data, 2);
  if (err != i2c::ERROR_OK)
    return true;
  uint8_t recv[2];
  err = this->read(recv, 2);
  if (err != i2c::ERROR_OK)
    return true;
  *value = encode_uint16(recv[0], recv[1]);
  return false;
}
bool AdE7953I2c::ade_read_32(uint16_t reg, uint32_t *value) {
  uint8_t reg_data[2];
  put_be16(reg_data, reg);
  i2c::ErrorCode err = this->write(reg_data, 2);
  if (err != i2c::ERROR_OK)
    return true;
  uint8_t recv[4];
  err = this->read(recv, 4);
  if (err != i2c::ERROR_OK)
    return true;
  *value = encode_uint32(recv[0], recv[1], recv[2], recv[3]);
  return false;
}

}  // namespace esphome::ade7953_i2c
