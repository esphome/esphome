#pragma once

#include <array>

#include "esphome/components/sx1509/sx1509.h"

namespace esphome::sx1509::testing {

/// Register file with the chip's auto-increment; the setup check and LED intensities hold their reset values.
class RegisterBus final : public i2c::I2CBus {
 public:
  RegisterBus() {
    this->registers[REG_INTERRUPT_MASK_A] = 0xFF;
    for (uint8_t reg : REG_I_ON)
      this->registers[reg] = 0xFF;
  }

  std::array<uint8_t, 256> registers{};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    uint8_t reg = size != 0 ? data[0] : 0;
    for (size_t i = 1; i < size; i++)
      this->registers[reg++] = data[i];
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->registers[reg++];
    return i2c::ERROR_OK;
  }

  uint16_t word(uint8_t reg_b) const { return (this->registers[reg_b] << 8) | this->registers[reg_b + 1]; }

  /// The chip reports a pressed row and column as cleared bits
  void press(uint16_t key_data) {
    const uint16_t raw = 0xFFFF ^ key_data;
    this->registers[REG_KEY_DATA_1] = raw >> 8;
    this->registers[REG_KEY_DATA_1 + 1] = raw & 0xFF;
  }
};

}  // namespace esphome::sx1509::testing
