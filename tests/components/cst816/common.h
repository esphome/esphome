#pragma once

#include <array>

#include <gtest/gtest.h>

#include "esphome/components/cst816/touchscreen/cst816_touchscreen.h"

namespace esphome::cst816::testing {

static constexpr uint8_t ADDRESS = 0x15;
// The driver reads REG_STATUS onwards in one transfer: status, gesture, touch count, then two points.
static constexpr size_t FRAME_SIZE = 13;

/// Serves one fixed status frame.
class FrameBus final : public i2c::I2CBus {
 public:
  explicit FrameBus(std::array<uint8_t, FRAME_SIZE> frame) : frame_(frame) {}

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    EXPECT_EQ(address, ADDRESS);
    for (size_t i = 0; i < read_count; i++)
      read_buffer[i] = i < FRAME_SIZE ? this->frame_[i] : 0;
    return i2c::ERROR_OK;
  }

 protected:
  std::array<uint8_t, FRAME_SIZE> frame_;
};

inline std::array<uint8_t, FRAME_SIZE> make_frame(uint8_t touch_num, uint16_t x = 0, uint16_t y = 0) {
  std::array<uint8_t, FRAME_SIZE> frame{};
  frame[REG_TOUCH_NUM] = touch_num;
  frame[REG_XPOS_HIGH] = (x >> 8) & 0x0F;
  frame[REG_XPOS_LOW] = x & 0xFF;
  frame[REG_YPOS_HIGH] = (y >> 8) & 0x0F;
  frame[REG_YPOS_LOW] = y & 0xFF;
  return frame;
}

}  // namespace esphome::cst816::testing
