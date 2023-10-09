#include "ade7953_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ade7953_i2c {

static const char *const TAG = "ade7953";

void AdE7953I2c::dump_config() {
  ESP_LOGCONFIG(TAG, "ADE7953_i2c:");
  LOG_I2C_DEVICE(this);
  ade7953_base::ADE7953::dump_config();
}
bool AdE7953I2c::ade_write_8(uint16_t reg, uint8_t value) {
  std::vector<uint8_t> data;
  data.push_back(reg >> 8);
  data.push_back(reg >> 0);
  data.push_back(value);
  return write(data.data(), data.size()) != i2c::ERROR_OK;
}
bool AdE7953I2c::ade_write_16(uint16_t reg, uint16_t value) {
  std::vector<uint8_t> data;
  data.push_back(reg >> 8);
  data.push_back(reg >> 0);
  data.push_back(value >> 8);
  data.push_back(value >> 0);
  return write(data.data(), data.size()) != i2c::ERROR_OK;
}
bool AdE7953I2c::ade_write_32(uint16_t reg, uint32_t value) {
  std::vector<uint8_t> data;
  data.push_back(reg >> 8);
  data.push_back(reg >> 0);
  data.push_back(value >> 24);
  data.push_back(value >> 16);
  data.push_back(value >> 8);
  data.push_back(value >> 0);
  return write(data.data(), data.size()) != i2c::ERROR_OK;
}
bool AdE7953I2c::ade_read_8(uint16_t reg, uint8_t *value) {
  uint8_t reg_data[2];
  reg_data[0] = reg >> 8;
  reg_data[1] = reg >> 0;
  i2c::ErrorCode err = write(reg_data, 2);
  if (err != i2c::ERROR_OK)
    return true;
  err = read(value, 1);
  return (err != i2c::ERROR_OK);
}
bool AdE7953I2c::ade_read_16(uint16_t reg, uint16_t *value) {
  uint8_t reg_data[2];
  reg_data[0] = reg >> 8;
  reg_data[1] = reg >> 0;
  i2c::ErrorCode err = write(reg_data, 2);
  if (err != i2c::ERROR_OK)
    return true;
  uint8_t recv[2];
  err = read(recv, 2);
  if (err != i2c::ERROR_OK)
    return true;
  *value = 0;
  *value |= ((uint32_t) recv[0]) << 8;
  *value |= ((uint32_t) recv[1]);
  return false;
}
bool AdE7953I2c::ade_read_32(uint16_t reg, uint32_t *value) {
  uint8_t reg_data[2];
  reg_data[0] = reg >> 8;
  reg_data[1] = reg >> 0;
  i2c::ErrorCode err = write(reg_data, 2);
  if (err != i2c::ERROR_OK)
    return true;
  uint8_t recv[4];
  err = read(recv, 4);
  if (err != i2c::ERROR_OK)
    return true;
  *value = 0;
  *value |= ((uint32_t) recv[0]) << 24;
  *value |= ((uint32_t) recv[1]) << 16;
  *value |= ((uint32_t) recv[2]) << 8;
  *value |= ((uint32_t) recv[3]);
  return false;
}

}  // namespace ade7953_i2c
}  // namespace esphome
