#pragma once

#include <cstring>

#include "esphome/components/ufire_ise/ufire_ise.h"

namespace esphome::ufire_ise::testing {

// Answers a 4-byte read of the millivolt register with `mv`, one byte per read
class MillivoltBus final : public i2c::I2CBus {
 public:
  float mv{0};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *out, size_t out_size, uint8_t *in,
                             size_t in_size) override {
    if (out_size > 0) {
      this->reg_ = out[0];
      this->offset_ = 0;
    }
    uint8_t bytes[4]{};
    if (this->reg_ == REGISTER_MV)
      std::memcpy(bytes, &this->mv, sizeof(bytes));
    for (size_t i = 0; i < in_size; i++)
      in[i] = bytes[this->offset_++ % 4];
    return i2c::ERROR_OK;
  }

 protected:
  uint8_t reg_{0};
  size_t offset_{0};
};

}  // namespace esphome::ufire_ise::testing
