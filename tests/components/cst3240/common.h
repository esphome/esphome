#pragma once

#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

#include "esphome/components/cst3240/touchscreen/cst3240_touchscreen.h"
#include "esphome/core/helpers.h"

namespace esphome::cst3240::testing {

struct Write {
  uint16_t reg;
  size_t size;
};

// Answers 16-bit register reads from a map; every write succeeds and is recorded.
class RegisterBus final : public i2c::I2CBus {
 public:
  std::map<uint16_t, std::vector<uint8_t>> registers;
  std::vector<Write> writes;

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    if (size < 2)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    const uint16_t reg = encode_uint16(data[0], data[1]);
    if (out_size == 0) {
      this->writes.push_back({reg, size});
      return i2c::ERROR_OK;
    }
    auto &value = this->registers[reg];
    value.resize(std::max(value.size(), out_size));
    std::memcpy(out, value.data(), out_size);
    return i2c::ERROR_OK;
  }

  // Lists `count` fingers with ids 0..count-1, each with the given event bits.
  void set_report(uint8_t count, uint8_t sync = cstxxx::SYNC_BYTE, uint8_t event = TOUCH_EVENT_PRESSED) {
    std::vector<uint8_t> data(CST3240_REPORT_SIZE);
    for (uint8_t i = 0; i < std::min(count, CST3240_MAX_TOUCHES); i++)
      data[i * 5 + (i == 0 ? 0 : 2)] = i << 4 | event;
    data[cstxxx::FINGER_COUNT_IDX] = count;
    data[cstxxx::SYNC_IDX] = sync;
    this->registers[cstxxx::REG_TOUCH_DATA] = data;
  }
};

}  // namespace esphome::cst3240::testing
