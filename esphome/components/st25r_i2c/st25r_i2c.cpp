#include "st25r_i2c.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cstring>

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
  auto err = this->i2c::I2CDevice::write_read(&addr, 1, &value, 1);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "read_register(0x%02X): I2C error %d", reg, (int) err);
  }
  return value;
}

void ST25RI2c::write_register(uint8_t reg, uint8_t value) {
  uint8_t data[2] = { (uint8_t)(0x00 | (reg & 0x3F)), value };
  auto err = this->i2c::I2CDevice::write(data, 2);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "write_register(0x%02X, 0x%02X): I2C error %d", reg, value, (int) err);
  }
}

void ST25RI2c::write_command(uint8_t command) {
  auto err = this->i2c::I2CDevice::write(&command, 1);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "write_command(0x%02X): I2C error %d", command, (int) err);
  }
}

void ST25RI2c::write_fifo(const uint8_t *data, size_t len) {
  static constexpr size_t MAX_FIFO_SIZE = 64;
  if (len > MAX_FIFO_SIZE) {
    ESP_LOGW(TAG, "write_fifo: len %u exceeds max %u, truncating", (unsigned) len, (unsigned) MAX_FIFO_SIZE);
    len = MAX_FIFO_SIZE;
  }
  uint8_t buf[MAX_FIFO_SIZE + 1];
  buf[0] = 0x80;
  memcpy(buf + 1, data, len);
  auto err = this->i2c::I2CDevice::write(buf, len + 1);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "write_fifo: I2C write failed (error %d)", (int) err);
  }
}

void ST25RI2c::read_fifo(uint8_t *data, size_t len) {
  uint8_t addr = 0x9F;
  auto err = this->i2c::I2CDevice::write_read(&addr, 1, data, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "read_fifo: I2C read failed (error %d, len=%u)", (int) err, (unsigned) len);
  }
}

}  // namespace st25r_i2c
}  // namespace esphome
