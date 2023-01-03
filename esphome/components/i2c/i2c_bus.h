#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>

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
  ERROR_CRC = 7,
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
    return write(address, buffer, len, true);
  }
  virtual ErrorCode write(uint8_t address, const uint8_t *buffer, size_t len, bool stop) {
    WriteBuffer buf;
    buf.data = buffer;
    buf.len = len;
    return writev(address, &buf, 1, stop);
  }
  virtual ErrorCode writev(uint8_t address, WriteBuffer *buffers, size_t cnt) {
    return writev(address, buffers, cnt, true);
  }
  virtual ErrorCode writev(uint8_t address, WriteBuffer *buffers, size_t cnt, bool stop) = 0;

 protected:
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
  std::vector<std::pair<uint8_t, bool>> scan_results_;
  bool scan_{false};
};

}  // namespace i2c
}  // namespace esphome
