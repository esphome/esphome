#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "esphome/core/helpers.h"

namespace esphome {
namespace i2c {

/// @brief Helper class for efficient buffer allocation - uses stack for small sizes, heap for large
template<size_t STACK_SIZE> class SmallBufferWithHeapFallback {
 public:
  uint8_t *get(size_t size) {
    if (size <= STACK_SIZE) {
      return this->stack_buffer_;
    }
    this->heap_buffer_ = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
    return this->heap_buffer_.get();
  }

 private:
  uint8_t stack_buffer_[STACK_SIZE];
  std::unique_ptr<uint8_t[]> heap_buffer_;
};

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
  virtual ~I2CBus() = default;

  /// @brief This virtual method writes bytes to an I2CBus from an array,
  /// then reads bytes into an array of ReadBuffer.
  /// @param address address of the I²C device on the i2c bus
  /// @param write_buffer pointer to data
  /// @param write_count number of bytes to write
  /// @param read_buffer pointer to an array to receive data
  /// @param read_count number of bytes to read
  /// transmission. False will send a restart, keeping the connection active.
  /// @return an i2c::ErrorCode
  /// @details This is a pure virtual method that must be implemented in the subclass.
  virtual ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                                size_t read_count) = 0;

  // Legacy functions for compatibility

  ErrorCode read(uint8_t address, uint8_t *buffer, size_t len) {
    return this->write_readv(address, nullptr, 0, buffer, len);
  }

  ErrorCode write(uint8_t address, const uint8_t *buffer, size_t len, bool stop = true) {
    return this->write_readv(address, buffer, len, nullptr, 0);
  }

  ESPDEPRECATED("This method is deprecated and will be removed in ESPHome 2026.3.0. Use write_readv() instead.",
                "2025.9.0")
  ErrorCode readv(uint8_t address, ReadBuffer *read_buffers, size_t count) {
    size_t total_len = 0;
    for (size_t i = 0; i != count; i++) {
      total_len += read_buffers[i].len;
    }

    SmallBufferWithHeapFallback<128> buffer_alloc;  // Most I2C reads are small
    uint8_t *buffer = buffer_alloc.get(total_len);

    auto err = this->write_readv(address, nullptr, 0, buffer, total_len);
    if (err != ERROR_OK)
      return err;
    size_t pos = 0;
    for (size_t i = 0; i != count; i++) {
      if (read_buffers[i].len != 0) {
        std::memcpy(read_buffers[i].data, buffer + pos, read_buffers[i].len);
        pos += read_buffers[i].len;
      }
    }
    return ERROR_OK;
  }

  ESPDEPRECATED("This method is deprecated and will be removed in ESPHome 2026.3.0. Use write_readv() instead.",
                "2025.9.0")
  ErrorCode writev(uint8_t address, const WriteBuffer *write_buffers, size_t count, bool stop = true) {
    size_t total_len = 0;
    for (size_t i = 0; i != count; i++) {
      total_len += write_buffers[i].len;
    }

    SmallBufferWithHeapFallback<128> buffer_alloc;  // Most I2C writes are small
    uint8_t *buffer = buffer_alloc.get(total_len);

    size_t pos = 0;
    for (size_t i = 0; i != count; i++) {
      std::memcpy(buffer + pos, write_buffers[i].data, write_buffers[i].len);
      pos += write_buffers[i].len;
    }

    return this->write_readv(address, buffer, total_len, nullptr, 0);
  }

 protected:
  /// @brief Scans the I2C bus for devices. Devices presence is kept in an array of std::pair
  /// that contains the address and the corresponding bool presence flag.
  void i2c_scan_();
  std::vector<std::pair<uint8_t, bool>> scan_results_;  ///< array containing scan results
  bool scan_{false};                                    ///< Should we scan ? Can be set in the yaml
};

class InternalI2CBus : public I2CBus {
 public:
  /// @brief Returns the I2C port number.
  /// @return the port number of the internal I2C bus
  virtual int get_port() const = 0;
};

}  // namespace i2c
}  // namespace esphome
