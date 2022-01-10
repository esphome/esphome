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
};

struct ReadBuffer {
  uint8_t *data;
  size_t len;
};
struct WriteBuffer {
  const uint8_t *data;
  size_t len;
};

struct Transaction {
  uint8_t device_address;
  const uint8_t *write_buf;
  size_t write_len;
  uint8_t *read_buf;
  size_t read_len;
};

class I2CBus {
 public:
  /// Most I2C (and SMBus) bus transactions follow the process of
  /// writing the command request to the device,
  /// then retreiving (reading) the reply. Between write and read the bus is not
  /// released (stop sent), rather a restart is sent keeping the command/reply atomic.
  /// Adopting this method means in the future the transactions can be queued and
  /// serviced on interrupts.
  virtual ErrorCode write_read(Transaction *t) = 0;
  /// Single treaded method
  virtual ErrorCode write_read(uint8_t address, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer,
                               size_t read_len) {
    Transaction ta;
    ta.device_address = address;
    ta.write_buf = write_buffer;
    ta.write_len = write_len;
    ta.read_buf = read_buffer;
    ta.read_len = read_len;
    return write_read(&ta);
  }

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
