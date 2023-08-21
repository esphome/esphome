#include "i2c.h"
#include "esphome/core/log.h"
#include <memory>

namespace esphome {
namespace i2c {

static const char *const TAG = "i2c";

ErrorCode I2CDevice::read_register(uint8_t a_register, uint8_t *data, size_t len, bool stop) {
  ErrorCode err = this->write(&a_register, 1, stop);
  if (err != ERROR_OK)
    return err;
  return bus_->read(address_, data, len);
}

ErrorCode I2CDevice::read_register16(uint16_t a_register, uint8_t *data, size_t len, bool stop) {
  a_register = convert_big_endian(a_register);
  ErrorCode const err = this->write(reinterpret_cast<const uint8_t *>(&a_register), 2, stop);
  if (err != ERROR_OK)
    return err;
  return bus_->read(address_, data, len);
}

ErrorCode I2CDevice::write_register(uint8_t a_register, const uint8_t *data, size_t len, bool stop) {
  WriteBuffer buffers[2];
  buffers[0].data = &a_register;
  buffers[0].len = 1;
  buffers[1].data = data;
  buffers[1].len = len;
  return bus_->writev(address_, buffers, 2, stop);
}

ErrorCode I2CDevice::write_register16(uint16_t a_register, const uint8_t *data, size_t len, bool stop) {
  a_register = convert_big_endian(a_register);
  WriteBuffer buffers[2];
  buffers[0].data = reinterpret_cast<const uint8_t *>(&a_register);
  buffers[0].len = 2;
  buffers[1].data = data;
  buffers[1].len = len;
  return bus_->writev(address_, buffers, 2, stop);
}

bool I2CDevice::read_bytes_16(uint8_t a_register, uint16_t *data, uint8_t len) {
  if (read_register(a_register, reinterpret_cast<uint8_t *>(data), len * 2) != ERROR_OK)
    return false;
  for (size_t i = 0; i < len; i++)
    data[i] = i2ctohs(data[i]);
  return true;
}

bool I2CDevice::write_bytes_16(uint8_t a_register, const uint16_t *data, uint8_t len) {
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
