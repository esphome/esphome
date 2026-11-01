#pragma once

#include <gtest/gtest.h>

#include <array>

#include "esphome/components/bq25186/bq25186.h"

namespace esphome::bq25186::testing {

static constexpr uint8_t ADDRESS = 0x6A;

// The chip's registers behind an address pointer that moves on after every byte; writes can be made to fail.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (size > 1 && this->fail_writes)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (out_size != 0 && this->fail_reads)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size != 0) {
      this->pointer_ = data[0];
      for (size_t i = 1; i < size; i++)
        this->registers[this->next_()] = data[i];
    }
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->registers[this->next_()];
    return i2c::ERROR_OK;
  }

  std::array<uint8_t, BQ25186_STATUS_REGISTER_COUNT> registers{};
  bool fail_writes{false};
  bool fail_reads{false};

 protected:
  size_t next_() { return this->pointer_++ % this->registers.size(); }

  uint8_t pointer_{0};
};

}  // namespace esphome::bq25186::testing
