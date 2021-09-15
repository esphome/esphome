#pragma once
#include <cstdint>
#include <cstddef>

namespace esphome {
namespace i2c {

enum ErrorCode {
  ERROR_OK = 0,
  ERROR_INVALID_ARGUMENT = 1,
  ERROR_NOT_ACKNOWLEDGED = 2,
  ERROR_TIMEOUT = 3,
  ERROR_NOT_INITIALIZED = 4,
  ERROR_TOO_LARGE = 5,
  ERROR_UNKNOWN = 6,
};

struct ReadBuffer {
  uint8_t *data;
  size_t len;
};
struct WriteBuffer {
  const uint8_t *data;
  size_t len;
};

class I2CBus {
 public:
  virtual ErrorCode read(uint8_t address, uint8_t *buffer, size_t len) {
    ReadBuffer buf;
    buf.data = buffer;
    buf.len = len;
    return readv(address, &buf, 1);
  }
  virtual ErrorCode readv(uint8_t address, ReadBuffer *buffers, size_t cnt) = 0;
  virtual ErrorCode write(uint8_t address, const uint8_t *buffer, size_t len) {
    WriteBuffer buf;
    buf.data = buffer;
    buf.len = len;
    return writev(address, &buf, 1);
  }
  virtual ErrorCode writev(uint8_t address, WriteBuffer *buffers, size_t cnt) = 0;
};

}  // namespace i2c
}  // namespace esphome
