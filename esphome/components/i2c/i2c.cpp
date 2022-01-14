#include "i2c.h"
#include "esphome/core/log.h"
#include <memory>

namespace esphome {
namespace i2c {

static const char *const TAG = "i2c";

bool I2CDevice::write_bytes_16(uint8_t a_register, const uint16_t *data, uint8_t len) {
  // we have to copy in order to be able to change byte order
  std::unique_ptr<uint16_t[]> temp{new uint16_t[len]};
  for (size_t i = 0; i < len; i++)
    temp[i] = htoi2cs(data[i]);
  return write_register(a_register, reinterpret_cast<const uint8_t *>(temp.get()), len * 2) == ERROR_OK;
}

template<typename TRegisterAddress, typename TRegisterValue>
I2CRegister<TRegisterAddress, TRegisterValue> &I2CRegister<TRegisterAddress, TRegisterValue>::operator=(
    TRegisterValue value) {
  this->parent_->write_register(this->register_, &value);
  return *this;
}

template<typename TRegisterAddress, typename TRegisterValue>
I2CRegister<TRegisterAddress, TRegisterValue> &I2CRegister<TRegisterAddress, TRegisterValue>::operator&=(
    TRegisterValue value) {
  value &= get();
  this->parent_->write_register(this->register_, &value);
  return *this;
}

template<typename TRegisterAddress, typename TRegisterValue>
I2CRegister<TRegisterAddress, TRegisterValue> &I2CRegister<TRegisterAddress, TRegisterValue>::operator|=(
    TRegisterValue value) {
  value |= get();
  this->parent_->write_register(this->register_, &value);
  return *this;
}

template<typename TRegisterAddress, typename TRegisterValue>
TRegisterValue I2CRegister<TRegisterAddress, TRegisterValue>::get() const {
  TRegisterValue value = 0x00;
  this->parent_->read_register(this->register_, &value);
  return value;
}

}  // namespace i2c
}  // namespace esphome
