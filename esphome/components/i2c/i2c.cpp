#include "i2c.h"

#include "esphome/core/log.h"
#include <memory>

namespace esphome {
namespace i2c {

static const char *const TAG = "i2c";

ErrorCode I2CDevice::read_register(uint8_t a_register, uint8_t *data, size_t len) {
  return bus_->write_readv(this->address_, &a_register, 1, data, len);
}

ErrorCode I2CDevice::read_register16(uint16_t a_register, uint8_t *data, size_t len) {
  a_register = convert_big_endian(a_register);
  return bus_->write_readv(this->address_, reinterpret_cast<const uint8_t *>(&a_register), 2, data, len);
}

ErrorCode I2CDevice::write_register(uint8_t a_register, const uint8_t *data, size_t len) const {
  std::vector<uint8_t> v(len + 1);
  v.push_back(a_register);
  v.insert(v.end(), data, data + len);
  return bus_->write_readv(this->address_, v.data(), v.size(), nullptr, 0);
}

ErrorCode I2CDevice::write_register16(uint16_t a_register, const uint8_t *data, size_t len) const {
  std::vector<uint8_t> v(len + 2);
  v.push_back(a_register >> 8);
  v.push_back(a_register);
  v.insert(v.end(), data, data + len);
  return bus_->write_readv(this->address_, v.data(), v.size(), nullptr, 0);
}

bool I2CDevice::read_bytes_16(uint8_t a_register, uint16_t *data, uint8_t len) {
  if (read_register(a_register, reinterpret_cast<uint8_t *>(data), len * 2) != ERROR_OK)
    return false;
  for (size_t i = 0; i < len; i++)
    data[i] = i2ctohs(data[i]);
  return true;
}

bool I2CDevice::write_bytes_16(uint8_t a_register, const uint16_t *data, uint8_t len) const {
  // we have to copy in order to be able to change byte order
  std::unique_ptr<uint16_t[]> temp{new uint16_t[len]};
  for (size_t i = 0; i < len; i++)
    temp[i] = htoi2cs(data[i]);
  return write_register(a_register, reinterpret_cast<const uint8_t *>(temp.get()), len * 2) == ERROR_OK;
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

}  // namespace i2c
}  // namespace esphome
