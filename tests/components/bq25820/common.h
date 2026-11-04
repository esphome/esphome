#pragma once

#include <gtest/gtest.h>

#include <array>

#include "esphome/components/bq25820/bq25820.h"

namespace esphome::bq25820::testing {

static constexpr uint8_t ADDRESS = 0x6B;
// The packed defaults with the watchdog off
static constexpr BQ25820Config DEFAULT_CONFIG{
    .limits = {0, 0, 0, 880, 1000},
    .controls = {0x10, 0x0A, 0x00, 0x00, 0x16, 0x01, 0x80, 0xC9},
    .ibat_scale = 0.002f,
    .iac_scale = 0.002f,
};

// The chip's registers behind an address pointer that moves on after every byte; reads, and writes to one
// register, can be made to fail.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (out_size != 0 && this->fail_reads)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size > 1 && data[0] == this->fail_write_register && this->writes_before_failure-- <= 0)
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

  std::array<uint8_t, 0x40> registers{};
  bool fail_reads{false};
  // Writes to this register fail once `writes_before_failure` of them have gone through
  int fail_write_register{-1};
  int writes_before_failure{0};

 protected:
  size_t next_() { return this->pointer_++ % this->registers.size(); }

  uint8_t pointer_{0};
};

}  // namespace esphome::bq25820::testing
