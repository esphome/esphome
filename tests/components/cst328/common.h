#pragma once

#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

#include "esphome/components/cst328/touchscreen/cst328_touchscreen.h"
#include "esphome/core/helpers.h"

namespace esphome::cst328::testing {

// Answers 16-bit register reads from a map; every write succeeds.
class RegisterBus final : public i2c::I2CBus {
 public:
  std::map<uint16_t, std::vector<uint8_t>> registers;

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    if (out_size == 0)
      return i2c::ERROR_OK;
    if (size < 2)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    auto &value = this->registers[encode_uint16(data[0], data[1])];
    value.resize(std::max(value.size(), out_size));
    std::memcpy(out, value.data(), out_size);
    return i2c::ERROR_OK;
  }

  void set_finger_count(uint8_t count) {
    std::vector<uint8_t> data(CST328_TOUCH_DATA_SIZE);
    data[CST_REG_FINGER_COUNT_IDX] = count;
    this->registers[CST_REG_TOUCH_INFORMATION] = data;
  }
};

}  // namespace esphome::cst328::testing
