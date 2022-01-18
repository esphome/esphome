#pragma once

#include "i2c_bus.h"
#include "esphome/core/helpers.h"
#include "esphome/core/optional.h"
#include <array>
#include <vector>

namespace esphome {
namespace i2c {

#define LOG_I2C_DEVICE(this) ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);

class I2CDevice;
template<typename TRegisterAddress = uint8_t, typename TRegisterValue = uint8_t> class I2CRegister {
 public:
  I2CRegister<TRegisterAddress, TRegisterValue> &operator=(TRegisterValue value);
  I2CRegister<TRegisterAddress, TRegisterValue> &operator&=(TRegisterValue value);
  I2CRegister<TRegisterAddress, TRegisterValue> &operator|=(TRegisterValue value);

  explicit operator TRegisterValue() const { return get(); }

  TRegisterValue get() const;

 protected:
  friend class I2CDevice;

  I2CRegister(I2CDevice *parent, TRegisterAddress a_register) : parent_(parent), register_(a_register) {}

  I2CDevice *parent_;
  TRegisterAddress register_;
};

// like ntohs/htons but without including networking headers.
// ("i2c" byte order is big-endian)
inline uint16_t i2ctohs(uint16_t i2cshort) { return convert_big_endian(i2cshort); }
inline uint16_t htoi2cs(uint16_t hostshort) { return convert_big_endian(hostshort); }

class I2CDevice {
 public:
  I2CDevice() = default;

  void set_i2c_address(uint8_t address) { address_ = address; }
  void set_i2c_bus(I2CBus *bus) { bus_ = bus; }

  I2CRegister<uint8_t, uint8_t> reg(uint8_t a_register) { return {this, a_register}; }

  template<typename TRegisterValue = uint8_t, typename TRegisterAddress = uint8_t>
  I2CRegister<TRegisterAddress, TRegisterValue> reg(TRegisterAddress a_register) {
    return {this, a_register};
  }

  ErrorCode read(uint8_t *data, size_t len) { return bus_->read(address_, data, len); }
  ErrorCode read_register(uint8_t a_register, uint8_t *data, size_t len) {
    ErrorCode err = this->write(&a_register, 1);
    if (err != ERROR_OK)
      return err;
    return this->read(data, len);
  }

  template<typename TRegisterAddress, typename TRegisterValue>
  ErrorCode read_register(TRegisterAddress a_register, TRegisterValue data) {
    uint8_t *reg = reinterpret_cast<uint8_t *>(convert_big_endian(&a_register));
    ErrorCode err = this->write(reg, sizeof(reg));
    if (err != ERROR_OK)
      return err;
    uint8_t *data2 = reinterpret_cast<uint8_t *>(convert_big_endian(&data));
    return this->read(data2, sizeof(data2));
  }

  ErrorCode write(const uint8_t *data, uint8_t len) { return bus_->write(address_, data, len); }
  ErrorCode write_register(uint8_t a_register, const uint8_t *data, size_t len) {
    WriteBuffer buffers[2];
    buffers[0].data = &a_register;
    buffers[0].len = 1;
    buffers[1].data = data;
    buffers[1].len = len;
    return bus_->writev(address_, buffers, 2);
  }

  template<typename TRegisterAddress, typename TRegisterValue>
  ErrorCode write_register(TRegisterAddress a_register, TRegisterValue data) {
    WriteBuffer buffers[2];
    buffers[0].data = reinterpret_cast<uint8_t *>(convert_big_endian(&a_register));
    buffers[0].len = sizeof(buffers[0].data);
    buffers[1].data = reinterpret_cast<uint8_t *>(convert_big_endian(&data));
    buffers[1].len = sizeof(buffers[1].data);
    return bus_->writev(address_, buffers, 2);
  }

  // Compat APIs

  bool read_bytes(uint8_t a_register, uint8_t *data, uint8_t len) {
    return read_register(a_register, data, len) == ERROR_OK;
  }
  bool read_bytes_raw(uint8_t *data, uint8_t len) { return read(data, len) == ERROR_OK; }

  template<size_t N> optional<std::array<uint8_t, N>> read_bytes(uint8_t a_register) {
    std::array<uint8_t, N> res;
    if (!this->read_bytes(a_register, res.data(), N)) {
      return {};
    }
    return res;
  }
  template<size_t N> optional<std::array<uint8_t, N>> read_bytes_raw() {
    std::array<uint8_t, N> res;
    if (!this->read_bytes_raw(res.data(), N)) {
      return {};
    }
    return res;
  }

  bool read_bytes_16(uint8_t a_register, uint16_t *data, uint8_t len) {
    if (read_register(a_register, reinterpret_cast<uint8_t *>(data), len * 2) != ERROR_OK)
      return false;
    for (size_t i = 0; i < len; i++)
      data[i] = i2ctohs(data[i]);
    return true;
  }

  bool read_byte(uint8_t a_register, uint8_t *data) { return read_register(a_register, data, 1) == ERROR_OK; }

  optional<uint8_t> read_byte(uint8_t a_register) {
    uint8_t data;
    if (!this->read_byte(a_register, &data))
      return {};
    return data;
  }

  bool read_byte_16(uint8_t a_register, uint16_t *data) { return read_bytes_16(a_register, data, 1); }

  bool write_bytes(uint8_t a_register, const uint8_t *data, uint8_t len) {
    return write_register(a_register, data, len) == ERROR_OK;
  }

  bool write_bytes(uint8_t a_register, const std::vector<uint8_t> &data) {
    return write_bytes(a_register, data.data(), data.size());
  }

  template<size_t N> bool write_bytes(uint8_t a_register, const std::array<uint8_t, N> &data) {
    return write_bytes(a_register, data.data(), data.size());
  }

  bool write_bytes_16(uint8_t a_register, const uint16_t *data, uint8_t len);

  bool write_byte(uint8_t a_register, uint8_t data) { return write_bytes(a_register, &data, 1); }

  bool write_byte_16(uint8_t a_register, uint16_t data) { return write_bytes_16(a_register, &data, 1); }

 protected:
  uint8_t address_{0x00};
  I2CBus *bus_{nullptr};
};

}  // namespace i2c
}  // namespace esphome
