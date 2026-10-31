#pragma once

#include <cstring>
#include <map>
#include <vector>

#include "esphome/components/gt911/touchscreen/gt911_touchscreen.h"
#include "esphome/core/helpers.h"

namespace esphome::gt911::testing {

// A 2-byte write selects a register that the next plain read returns; longer writes are ignored.
class RegisterBus final : public i2c::I2CBus {
 public:
  std::map<uint16_t, std::vector<uint8_t>> registers;
  uint16_t pointer{0};
  bool fail{false};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    if (this->fail)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size == 2)
      this->pointer = encode_uint16(data[0], data[1]);
    if (out_size != 0) {
      auto &value = this->registers[this->pointer];
      value.resize(std::max(value.size(), out_size));
      std::memcpy(out, value.data(), out_size);
    }
    return i2c::ERROR_OK;
  }

  // A ready report with no touch points and the given key bits
  void set_keys(uint8_t keys) {
    this->registers[0x814E] = {0x80};
    this->registers[0x814F] = {keys};
  }
};

}  // namespace esphome::gt911::testing
