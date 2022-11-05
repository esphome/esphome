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
class I2CRegister {
 public:
  I2CRegister &operator=(uint8_t value);
  I2CRegister &operator&=(uint8_t value);
  I2CRegister &operator|=(uint8_t value);

  explicit operator uint8_t() const { return get(); }

  uint8_t get() const;

 protected:
  friend class I2CDevice;

  I2CRegister(I2CDevice *parent, uint8_t a_register) : parent_(parent), register_(a_register) {}

  I2CDevice *parent_;
  uint8_t register_;
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

  I2CRegister reg(uint8_t a_register) { return {this, a_register}; }

  ErrorCode read(uint8_t *data, size_t len) { return bus_->read(address_, data, len); }
  ErrorCode read_register(uint8_t a_register, uint8_t *data, size_t len, bool stop = true) {
    ErrorCode err = this->write(&a_register, 1, stop);
    if (err != ERROR_OK)
      return err;
    return this->read(data, len);
  }

  ErrorCode write(const uint8_t *data, uint8_t len, bool stop = true) { return bus_->write(address_, data, len, stop); }
  ErrorCode write_register(uint8_t a_register, const uint8_t *data, size_t len, bool stop = true) {
    WriteBuffer buffers[2];
    buffers[0].data = &a_register;
    buffers[0].len = 1;
    buffers[1].data = data;
    buffers[1].len = len;
    return bus_->writev(address_, buffers, 2, stop);
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

  bool read_byte(uint8_t a_register, uint8_t *data, bool stop = true) {
    return read_register(a_register, data, 1, stop) == ERROR_OK;
  }

  optional<uint8_t> read_byte(uint8_t a_register) {
    uint8_t data;
    if (!this->read_byte(a_register, &data))
      return {};
    return data;
  }

  bool read_byte_16(uint8_t a_register, uint16_t *data) { return read_bytes_16(a_register, data, 1); }

  bool write_bytes(uint8_t a_register, const uint8_t *data, uint8_t len, bool stop = true) {
    return write_register(a_register, data, len, stop) == ERROR_OK;
  }

  bool write_bytes(uint8_t a_register, const std::vector<uint8_t> &data) {
    return write_bytes(a_register, data.data(), data.size());
  }

  template<size_t N> bool write_bytes(uint8_t a_register, const std::array<uint8_t, N> &data) {
    return write_bytes(a_register, data.data(), data.size());
  }

  bool write_bytes_16(uint8_t a_register, const uint16_t *data, uint8_t len);

  bool write_byte(uint8_t a_register, uint8_t data, bool stop = true) {
    return write_bytes(a_register, &data, 1, stop);
  }

  bool write_byte_16(uint8_t a_register, uint16_t data) { return write_bytes_16(a_register, &data, 1); }

 protected:
  uint8_t address_{0x00};
  I2CBus *bus_{nullptr};
};

}  // namespace i2c
}  // namespace esphome
