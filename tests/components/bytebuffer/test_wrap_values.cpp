#ifdef USE_HOST
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/bytebuffer/bytebuffer.h"

namespace esphome::bytebuffer {

// The expressions esp32_ble_server emits for typed constant values, and the bytes they produce.
// Doubles are written as float literals, so they carry float precision.
TEST(ByteBufferWrapValues, MatchesGeneratedConstants) {
  EXPECT_EQ(ByteBuffer::wrap(uint8_t(18), LITTLE).get_data(), (std::vector<uint8_t>{0x12}));
  EXPECT_EQ(ByteBuffer::wrap(uint16_t(18), LITTLE).get_data(), (std::vector<uint8_t>{0x12, 0x00}));
  EXPECT_EQ(ByteBuffer::wrap(uint16_t(4660), BIG).get_data(), (std::vector<uint8_t>{0x12, 0x34}));
  EXPECT_EQ(ByteBuffer::wrap(uint32_t(305419896), LITTLE).get_data(), (std::vector<uint8_t>{0x78, 0x56, 0x34, 0x12}));
  EXPECT_EQ(ByteBuffer::wrap(uint64_t(1311768467294899695ULL), BIG).get_data(),
            (std::vector<uint8_t>{0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef}));
  EXPECT_EQ(ByteBuffer::wrap(int8_t(-5), LITTLE).get_data(), (std::vector<uint8_t>{0xfb}));
  EXPECT_EQ(ByteBuffer::wrap(int16_t(-2), BIG).get_data(), (std::vector<uint8_t>{0xff, 0xfe}));
  EXPECT_EQ(ByteBuffer::wrap(int32_t(-2), LITTLE).get_data(), (std::vector<uint8_t>{0xfe, 0xff, 0xff, 0xff}));
  EXPECT_EQ(ByteBuffer::wrap(int64_t(-2), LITTLE).get_data(),
            (std::vector<uint8_t>{0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}));
  EXPECT_EQ(ByteBuffer::wrap(float(123.1f), BIG).get_data(), (std::vector<uint8_t>{0x42, 0xf6, 0x33, 0x33}));
  EXPECT_EQ(ByteBuffer::wrap(float(0.1f), LITTLE).get_data(), (std::vector<uint8_t>{0xcd, 0xcc, 0xcc, 0x3d}));
  EXPECT_EQ(ByteBuffer::wrap(double(0.1f), LITTLE).get_data(),
            (std::vector<uint8_t>{0x00, 0x00, 0x00, 0xa0, 0x99, 0x99, 0xb9, 0x3f}));
  EXPECT_EQ(ByteBuffer::wrap(double(2.5f), BIG).get_data(),
            (std::vector<uint8_t>{0x40, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
}

}  // namespace esphome::bytebuffer
#endif
