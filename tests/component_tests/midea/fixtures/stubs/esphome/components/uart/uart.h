#pragma once
#include <cstddef>
#include <cstdint>
namespace esphome::uart {
class UARTComponent {
 public:
  int available() { return 0; }
  bool read_byte(uint8_t *) { return false; }
  bool peek_byte(uint8_t *) { return false; }
  void write_byte(uint8_t) {}
  void write_array(const uint8_t *, size_t) {}
  void flush() {}
};
}  // namespace esphome::uart
