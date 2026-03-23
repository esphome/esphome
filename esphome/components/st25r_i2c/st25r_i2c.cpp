#include "st25r_i2c.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome {
namespace st25r_i2c {

static const char *const TAG = "st25r_i2c";

void ST25RI2c::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ST25R I2C...");

  // Wake up chip - send a dummy byte and ignore the result
  uint8_t dummy = 0x00;
  this->i2c::I2CDevice::write(&dummy, 1);
  delay(10);

  st25r::ST25R::setup();
}

void ST25RI2c::dump_config() {
  st25r::ST25R::dump_config();
  LOG_I2C_DEVICE(this);
}

uint8_t ST25RI2c::read_register(uint8_t reg) {
  uint8_t value = 0;
  uint8_t addr = 0x40 | (reg & 0x3F);
  this->i2c::I2CDevice::write_read(&addr, 1, &value, 1);
  return value;
}

void ST25RI2c::write_register(uint8_t reg, uint8_t value) {
  uint8_t data[2] = { (uint8_t)(0x00 | (reg & 0x3F)), value };
  this->i2c::I2CDevice::write(data, 2);
}

void ST25RI2c::write_command(uint8_t command) {
  this->i2c::I2CDevice::write(&command, 1);
}

void ST25RI2c::write_fifo(const uint8_t *data, size_t len) {
  std::vector<uint8_t> buf;
  buf.reserve(len + 1);
  buf.push_back(0x80);
  buf.insert(buf.end(), data, data + len);
  this->i2c::I2CDevice::write(buf.data(), buf.size());
}

void ST25RI2c::read_fifo(uint8_t *data, size_t len) {
  uint8_t addr = 0x9F;
  this->i2c::I2CDevice::write_read(&addr, 1, data, len);
}

}  // namespace st25r_i2c
}  // namespace esphome
