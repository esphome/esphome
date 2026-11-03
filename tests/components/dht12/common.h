#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "esphome/components/i2c/i2c_bus.h"

namespace esphome::dht12::testing {

static constexpr uint8_t ADDRESS = 0x5C;

// Answers every read with the five bytes in `frame`, starting at register 0.
class FrameBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    if (address != ADDRESS || size != 1 || data[0] != 0 || out_size > this->frame.size())
      return i2c::ERROR_UNKNOWN;
    std::copy_n(this->frame.begin(), out_size, out);
    return i2c::ERROR_OK;
  }

  std::array<uint8_t, 5> frame{};
};

}  // namespace esphome::dht12::testing
