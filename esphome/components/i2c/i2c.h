#pragma once

#include "i2c_bus.h"
#include "esphome/core/helpers.h"
#include "esphome/core/optional.h"
#include <array>
#include <vector>

namespace esphome {
namespace i2c {

#define LOG_I2C_DEVICE(this) ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);

class I2CDevice;  // forward declaration

/// @brief This class defines 'local 8 bits registers' that act as proxies to 'i2c remote device registers'
/// @details A proxy is a structural design pattern that lets you provide a substitute for another object.
/// Once you have define a local "proxy" register you can use the overriden operators as if you were doing
/// operations remotely.
/// @n typical usage:
/// @code
/// i2c::I2CRegister reg_1 = this->reg(ADDR_REGISTER_1); // declare
/// reg_1 |= 0x01; // set bit
/// reg_1 &= ~0x01; // reset bit
/// reg_1 = 10; // Set value
/// uint val = reg_1.get(); // get value
/// @endcode
class I2CRegister {
 public:
  /// @brief overloads the = operator. This is used to set a value in the register
  /// @param value to be set
  /// @return this object
  I2CRegister &operator=(uint8_t value);

  /// @brief overloads the compound &= operator. This is often used to reset bits in the register
  /// @param value performs an & operation with value and store the result
  /// @return this object
  I2CRegister &operator&=(uint8_t value);

  /// @brief overloads the compound |= operator. This is often used to set bits in the register
  /// @param value performs an | operation with value and store the result
  /// @return this object
  I2CRegister &operator|=(uint8_t value);

  /// @brief overloads the cast operator is used to return the register value
  explicit operator uint8_t() const { return get(); }

  /// @brief returns the register value
  /// @return the value
  uint8_t get() const;

 protected:
  friend class I2CDevice;

  /// @brief protected ctor. Only friends can create an I2CRegister
  /// @param parent our parent
  /// @param a_register address of the i2c register
  I2CRegister(I2CDevice *parent, uint8_t a_register) : parent_(parent), register_(a_register) {}

  I2CDevice *parent_;  ///< parent we belongs to
  uint8_t register_;   ///< the address of the register
};

/// @brief This class defines 'local 16 bits registers' that act as proxies to 'i2c remote device registers'
/// @details works exactly as I2CRegister but on 16 bits registers. For more information see the I2CRegister class.
class I2CRegister16 {
 public:
  /// @brief overloads the = operator. This is used to set a value in the register
  /// @todo @bug wrong?
  /// @param value to be set
  /// @return this object
  I2CRegister16 &operator=(uint8_t value);

  /// @brief overloads the compound &= operator. This is often used to reset bits in the register
  /// @param value performs an & operation with value and store the result
  /// @return this object
  I2CRegister16 &operator&=(uint8_t value);

  /// @brief overloads the compound |= operator. This is often used to set bits in the register
  /// @param value performs an | operation with value and store the result
  /// @return this object
  I2CRegister16 &operator|=(uint8_t value);

  /// @brief overloads the cast operator is used to return the register value
  explicit operator uint8_t() const { return get(); }

  /// @brief returns the register value
  /// @return the value
  uint8_t get() const;

 protected:
  friend class I2CDevice;

  /// @brief protected ctor. Only friends can create an I2CRegister
  /// @param parent our parent
  /// @param a_register the register to work on
  I2CRegister16(I2CDevice *parent, uint16_t a_register) : parent_(parent), register_(a_register) {}

  I2CDevice *parent_;  ///< parent we belongs to
  uint16_t register_;  ///< the address of the register
};

// like ntohs/htons but without including networking headers.
// ("i2c" byte order is big-endian)
inline uint16_t i2ctohs(uint16_t i2cshort) { return convert_big_endian(i2cshort); }
inline uint16_t htoi2cs(uint16_t hostshort) { return convert_big_endian(hostshort); }

/// @brief This Class provides the methods to read and write bytes to/from an i2c device.
/// it also keeps the address of the device as well as a pointer to the I2CBus used.
class I2CDevice {
 public:
  /// @brief we use the C++ default ctor
  I2CDevice() = default;

  /// @brief We store the address of the device on the bus
  /// @param address to store
  void set_i2c_address(uint8_t address) { address_ = address; }

  /// @brief we store the pointer to the I2CBus to use
  /// @param bus
  void set_i2c_bus(I2CBus *bus) { bus_ = bus; }

  /// @brief call the I2CRegister ctor
  /// @param a_register address of the register
  /// @return an I2CRegister proxy to the register at a_address
  I2CRegister reg(uint8_t a_register) { return {this, a_register}; }

  /// @brief @bug @todo seems not correct
  /// @param a_register
  /// @return
  I2CRegister16 reg16(uint16_t a_register) { return {this, a_register}; }

  /// @brief read an array of bytes on the I2CBus
  /// @param data pointer to an array to store the bytes
  /// @param len length of the buffer = number of bytes to read
  /// @return an i2c::ErrorCode
  ErrorCode read(uint8_t *data, size_t len) { return bus_->read(address_, data, len); }

  /// @brief Reads an array of bytes from a specific register
  /// @param a_register the address of the register to read from
  /// @param data pointer to an array to store the bytes
  /// @param len length of the buffer = number of bytes to read
  /// @param stop (true/false): True will send a stop message, releasing the bus after
  /// transmission. False will send a restart, keeping the connection active.
  /// @return an i2c::ErrorCode
  ErrorCode read_register(uint8_t a_register, uint8_t *data, size_t len, bool stop = true);

  /// @brief  @todo @bug seems not correct
  /// @param a_register
  /// @param data
  /// @param len
  /// @param stop
  /// @return
  ErrorCode read_register16(uint16_t a_register, uint8_t *data, size_t len, bool stop = true);

  /// @brief write an array of bytes on the I2CBus
  /// @param data pointer to an array that contains the bytes to send
  /// @param len length of the buffer = number of bytes to write
  /// @return an i2c::ErrorCode
  ErrorCode write(const uint8_t *data, size_t len, bool stop = true) { return bus_->write(address_, data, len, stop); }

  /// @brief Reads an array of bytes from a specific register
  /// @param a_register the address of the register to read from
  /// @param data pointer to an array to store the bytes
  /// @param len length of the buffer = number of bytes to read
  /// @param stop (true/false): True will send a stop message, releasing the bus after
  /// transmission. False will send a restart, keeping the connection active.
  /// @return an i2c::ErrorCode
  ErrorCode write_register(uint8_t a_register, const uint8_t *data, size_t len, bool stop = true);

  /// @todo seems bad @bug
  ErrorCode write_register16(uint16_t a_register, const uint8_t *data, size_t len, bool stop = true);

  ///
  /// Compat APIs
  /// All methods below seems to be here for compatibility reason
  /// From what I have seen none of them bring any functionality.
  /// Therefore they are not docummented and I would not recommend
  /// to use them.
  ///

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

  bool read_bytes_16(uint8_t a_register, uint16_t *data, uint8_t len);

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
  uint8_t address_{0x00};  ///< store the address of the device on the bus
  I2CBus *bus_{nullptr};   ///< pointer to I2CBus instance
};

}  // namespace i2c
}  // namespace esphome
