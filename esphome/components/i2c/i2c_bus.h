#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>

namespace esphome {
namespace i2c {

/// @brief Error codes returned by I2CBus and I2CDevice methods
enum ErrorCode {
  NO_ERROR = 0,                ///< No error found during execution of method
  ERROR_OK = 0,                ///< No error found during execution of method
  ERROR_INVALID_ARGUMENT = 1,  ///< method called invalid argument(s)
  ERROR_NOT_ACKNOWLEDGED = 2,  ///< I2C bus acknowledgment not received
  ERROR_TIMEOUT = 3,           ///< timeout while waiting to receive bytes
  ERROR_NOT_INITIALIZED = 4,   ///< call method to a not initialized bus
  ERROR_TOO_LARGE = 5,         ///< requested a transfer larger than buffers can hold
  ERROR_UNKNOWN = 6,           ///< miscellaneous I2C error during execution
  ERROR_CRC = 7,               ///< bytes received with a CRC error
};

/// @brief the ReadBuffer structure stores a pointer to a read buffer and its length
struct ReadBuffer {
  uint8_t *data;  ///< pointer to the read buffer
  size_t len;     ///< length of the buffer
};

/// @brief the WriteBuffer structure stores a pointer to a write buffer and its length
struct WriteBuffer {
  const uint8_t *data;  ///< pointer to the write buffer
  size_t len;           ///< length of the buffer
};

/// @brief This Class provides the methods to read and write bytes from an I2CBus.
/// @note The I2CBus virtual class follows a *Factory design pattern* that provides all the interfaces methods required
/// by clients while deferring the actual implementation of these methods to a subclasses. I2C-bus specification and
/// user manual can be found here https://www.nxp.com/docs/en/user-guide/UM10204.pdf and an interesting I²C Application
/// note https://www.nxp.com/docs/en/application-note/AN10216.pdf
class I2CBus {
 public:
  /// @brief Creates a ReadBuffer and calls the virtual readv() method to read bytes into this buffer
  /// @param address address of the I²C component on the i2c bus
  /// @param buffer pointer to an array of bytes that will be used to store the data received
  /// @param len length of the buffer = number of bytes to read
  /// @return an i2c::ErrorCode
  virtual ErrorCode read(uint8_t address, uint8_t *buffer, size_t len) {
    ReadBuffer buf;
    buf.data = buffer;
    buf.len = len;
    return readv(address, &buf, 1);
  }

  /// @brief This virtual method reads bytes from an I2CBus into an array of ReadBuffer.
  /// @param address address of the I²C component on the i2c bus
  /// @param buffers pointer to an array of ReadBuffer
  /// @param count number of ReadBuffer to read
  /// @return an i2c::ErrorCode
  /// @details This is a pure virtual method that must be implemented in a subclass.
  virtual ErrorCode readv(uint8_t address, ReadBuffer *buffers, size_t count) = 0;

  virtual ErrorCode write(uint8_t address, const uint8_t *buffer, size_t len) {
    return write(address, buffer, len, true);
  }

  /// @brief Creates a WriteBuffer and calls the writev() method to send the bytes from this buffer
  /// @param address address of the I²C component on the i2c bus
  /// @param buffer pointer to an array of bytes that contains the data to be sent
  /// @param len length of the buffer = number of bytes to write
  /// @param stop true or false: True will send a stop message, releasing the bus after
  /// transmission. False will send a restart, keeping the connection active.
  /// @return an i2c::ErrorCode
  virtual ErrorCode write(uint8_t address, const uint8_t *buffer, size_t len, bool stop) {
    WriteBuffer buf;
    buf.data = buffer;
    buf.len = len;
    return writev(address, &buf, 1, stop);
  }

  virtual ErrorCode writev(uint8_t address, WriteBuffer *buffers, size_t cnt) {
    return writev(address, buffers, cnt, true);
  }

  /// @brief This virtual method writes bytes to an I2CBus from an array of WriteBuffer.
  /// @param address address of the I²C component on the i2c bus
  /// @param buffers pointer to an array of WriteBuffer
  /// @param count number of WriteBuffer to write
  /// @param stop true or false: True will send a stop message, releasing the bus after
  /// transmission. False will send a restart, keeping the connection active.
  /// @return an i2c::ErrorCode
  /// @details This is a pure virtual method that must be implemented in the subclass.
  virtual ErrorCode writev(uint8_t address, WriteBuffer *buffers, size_t count, bool stop) = 0;

 protected:
  /// @brief Scans the I2C bus for devices. Devices presence is kept in an array of std::pair
  /// that contains the address and the corresponding bool presence flag.
  void i2c_scan_() {
    for (uint8_t address = 8; address < 120; address++) {
      auto err = writev(address, nullptr, 0);
      if (err == ERROR_OK) {
        scan_results_.emplace_back(address, true);
      } else if (err == ERROR_UNKNOWN) {
        scan_results_.emplace_back(address, false);
      }
    }
  }
  std::vector<std::pair<uint8_t, bool>> scan_results_;  ///< array containing scan results
  bool scan_{false};                                    ///< Should we scan ? Can be set in the yaml
};

}  // namespace i2c
}  // namespace esphome
