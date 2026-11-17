#pragma once

#include <algorithm>
#include <array>
#include <cstring>

#include "esphome/core/gpio.h"
#include "esphome/components/cst226/touchscreen/cst226_touchscreen.h"

namespace esphome::cst226::testing {

// A reset pin that only records its level.
class FakePin final : public GPIOPin {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_OUTPUT; }
  bool digital_read() override { return this->level; }
  void digital_write(bool value) override { this->level = value; }
  bool level{false};
};

// Fills a 16-bit register read: the firmware check code read gets a valid code, other registers zeros.
inline void answer_setup_read(const uint8_t *data, size_t size, uint8_t *out, size_t out_size) {
  std::memset(out, 0, out_size);
  if (size == 2 && encode_uint16(data[0], data[1]) == cstxxx::REG_FW_CHECK_CODE && out_size == 4)
    out[2] = out[3] = 0xCA;
}

// Answers the 8-bit status read with a set report and 16-bit setup reads through answer_setup_read().
class StatusBus final : public i2c::I2CBus {
 public:
  std::array<uint8_t, 28> status{};
  bool fail{false};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    if (this->fail)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (out_size == 0)
      return i2c::ERROR_OK;
    if (size == 1 && data[0] == CST226_REG_STATUS) {
      std::memcpy(out, this->status.data(), std::min(out_size, this->status.size()));
    } else {
      answer_setup_read(data, size, out, out_size);
    }
    return i2c::ERROR_OK;
  }

  void set_key(bool pressed) {
    this->status = {};
    if (pressed) {
      this->status[0] = 0x83;
      this->status[1] = 0x17;
      this->status[5] = 0x80;
    } else {
      this->status[6] = 0xAB;
    }
  }
};

}  // namespace esphome::cst226::testing
