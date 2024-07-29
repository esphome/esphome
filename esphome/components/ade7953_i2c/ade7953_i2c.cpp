#include "ade7953_i2c.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ade7953_i2c {

static const char *const TAG = "ade7953";

void AdE7953I2c::dump_config() {
  ESP_LOGCONFIG(TAG, "ADE7953_i2c:");
  LOG_I2C_DEVICE(this);
  ade7953_base::ADE7953::dump_config();
}
bool AdE7953I2c::ade_write_8(uint16_t reg, uint8_t value) {
  uint8_t data[3];
  data[0] = reg >> 8;
  data[1] = reg >> 0;
  data[2] = value;
  return this->write(data, 3) != i2c::ERROR_OK;
}
bool AdE7953I2c::ade_write_16(uint16_t reg, uint16_t value) {
  uint8_t data[4];
  data[0] = reg >> 8;
  data[1] = reg >> 0;
  data[2] = value >> 8;
  data[3] = value >> 0;
  return this->write(data, 4) != i2c::ERROR_OK;
}
bool AdE7953I2c::ade_write_32(uint16_t reg, uint32_t value) {
  uint8_t data[6];
  data[0] = reg >> 8;
  data[1] = reg >> 0;
  data[2] = value >> 24;
  data[3] = value >> 16;
  data[4] = value >> 8;
  data[5] = value >> 0;
  return this->write(data, 6) != i2c::ERROR_OK;
}
bool AdE7953I2c::ade_read_8(uint16_t reg, uint8_t *value) {
  uint8_t reg_data[2];
  reg_data[0] = reg >> 8;
  reg_data[1] = reg >> 0;
  i2c::ErrorCode err = this->write(reg_data, 2);
  if (err != i2c::ERROR_OK)
    return true;
  err = this->read(value, 1);
  return (err != i2c::ERROR_OK);
}
bool AdE7953I2c::ade_read_16(uint16_t reg, uint16_t *value) {
  uint8_t reg_data[2];
  reg_data[0] = reg >> 8;
  reg_data[1] = reg >> 0;
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
  reg_data[0] = reg >> 8;
  reg_data[1] = reg >> 0;
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

}  // namespace ade7953_i2c
}  // namespace esphome
