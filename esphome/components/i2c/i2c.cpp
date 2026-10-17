#include "i2c.h"

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::i2c {

ESPHOME_LOG_TAG(TAG, "i2c");

void I2CBus::i2c_scan_() {
  for (uint8_t address = 8; address != 120; address++) {
    auto err = write_readv(address, nullptr, 0, nullptr, 0);
    if (err == ERROR_OK) {
      scan_results_.emplace_back(address, true);
    } else if (err == ERROR_UNKNOWN) {
      scan_results_.emplace_back(address, false);
    }
    // it takes 16sec to scan on nrf52. It prevents board reset.
    arch_feed_wdt();
  }
}

void log_wrong_chip_id(uint8_t id, uint8_t expected) {
  ESP_LOGE(TAG, "Wrong chip ID 0x%02X, expected 0x%02X", id, expected);
}

ErrorCode I2CDevice::read_register(uint8_t a_register, uint8_t *data, size_t len) {
  return bus_->write_readv(this->address_, &a_register, 1, data, len);
}

ErrorCode I2CDevice::read_register16(uint16_t a_register, uint8_t *data, size_t len) {
  a_register = convert_big_endian(a_register);
  return bus_->write_readv(this->address_, reinterpret_cast<const uint8_t *>(&a_register), 2, data, len);
}

ErrorCode I2CDevice::write_register(uint8_t a_register, const uint8_t *data, size_t len) const {
  SmallBufferWithHeapFallback<17> buffer_alloc(len + 1);  // Most I2C writes are <= 16 bytes
  uint8_t *buffer = buffer_alloc.get();

  buffer[0] = a_register;
  std::copy(data, data + len, buffer + 1);
  return this->bus_->write_readv(this->address_, buffer, len + 1, nullptr, 0);
}

ErrorCode I2CDevice::write_register16(uint16_t a_register, const uint8_t *data, size_t len) const {
  SmallBufferWithHeapFallback<18> buffer_alloc(len + 2);  // Most I2C writes are <= 16 bytes + 2 for register
  uint8_t *buffer = buffer_alloc.get();

  buffer[0] = a_register >> 8;
  buffer[1] = a_register;
  std::copy(data, data + len, buffer + 2);
  return this->bus_->write_readv(this->address_, buffer, len + 2, nullptr, 0);
}

bool I2CDevice::read_bytes_16(uint8_t a_register, uint16_t *data, uint8_t len) {
  if (read_register(a_register, reinterpret_cast<uint8_t *>(data), len * 2) != ERROR_OK)
    return false;
  for (size_t i = 0; i < len; i++)
    data[i] = i2ctohs(data[i]);
  return true;
}

bool I2CDevice::update_register(uint8_t a_register, uint8_t mask, uint8_t value) {
  uint8_t data;
  if (!this->read_byte(a_register, &data))
    return false;
  const uint8_t updated = (data & ~mask) | (value & mask);
  ESP_LOGV(TAG, "Register 0x%02X: 0x%02X -> 0x%02X", a_register, data, updated);
  return this->write_byte(a_register, updated);
}

bool I2CDevice::write_bytes_16(uint8_t a_register, const uint16_t *data, uint8_t len) const {
  const size_t size = 1 + len * 2;
  SmallBufferWithHeapFallback<17> buffer_alloc(size);  // Register plus up to 8 words
  uint8_t *buffer = buffer_alloc.get();

  buffer[0] = a_register;
  for (size_t i = 0; i < len; i++) {
    const uint16_t word = data[i];
    buffer[1 + i * 2] = word >> 8;
    buffer[2 + i * 2] = word;
  }
  return this->bus_->write_readv(this->address_, buffer, size, nullptr, 0) == ERROR_OK;
}

bool I2CDevice::write_byte_16(uint8_t a_register, uint16_t data) const {
  const uint8_t buffer[3] = {a_register, static_cast<uint8_t>(data >> 8), static_cast<uint8_t>(data)};
  return this->bus_->write_readv(this->address_, buffer, sizeof(buffer), nullptr, 0) == ERROR_OK;
}

I2CRegister &I2CRegister::operator=(uint8_t value) {
  this->parent_->write_register(this->register_, &value, 1);
  return *this;
}
I2CRegister &I2CRegister::operator&=(uint8_t value) {
  value &= get();
  this->parent_->write_register(this->register_, &value, 1);
  return *this;
}
I2CRegister &I2CRegister::operator|=(uint8_t value) {
  value |= get();
  this->parent_->write_register(this->register_, &value, 1);
  return *this;
}

uint8_t I2CRegister::get() const {
  uint8_t value = 0x00;
  this->parent_->read_register(this->register_, &value, 1);
  return value;
}

I2CRegister16 &I2CRegister16::operator=(uint8_t value) {
  this->parent_->write_register16(this->register_, &value, 1);
  return *this;
}
I2CRegister16 &I2CRegister16::operator&=(uint8_t value) {
  value &= get();
  this->parent_->write_register16(this->register_, &value, 1);
  return *this;
}
I2CRegister16 &I2CRegister16::operator|=(uint8_t value) {
  value |= get();
  this->parent_->write_register16(this->register_, &value, 1);
  return *this;
}

uint8_t I2CRegister16::get() const {
  uint8_t value = 0x00;
  this->parent_->read_register16(this->register_, &value, 1);
  return value;
}

}  // namespace esphome::i2c
