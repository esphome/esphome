#pragma once

#include <cstring>
#include <vector>

#include "esphome/components/i2c/i2c_bus.h"

namespace esphome::ufire_ec::testing {

// Records every write and answers reads with zeros
class RecordingBus final : public i2c::I2CBus {
 public:
  std::vector<std::vector<uint8_t>> writes;

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *out, size_t out_size, uint8_t *in,
                             size_t in_size) override {
    if (out_size > 0)
      this->writes.emplace_back(out, out + out_size);
    if (in_size > 0)
      std::memset(in, 0, in_size);
    return i2c::ERROR_OK;
  }

  size_t count(uint8_t reg) const {
    size_t n = 0;
    for (const auto &w : this->writes)
      n += w[0] == reg;
    return n;
  }
};

}  // namespace esphome::ufire_ec::testing
