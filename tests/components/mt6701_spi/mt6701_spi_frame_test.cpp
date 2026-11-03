#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "esphome/components/mt6701_spi/mt6701_spi.h"

namespace esphome::mt6701_spi::testing {

namespace {

// The datasheet's CRC: x^6 + x + 1 over the 18 data bits, most significant bit first, zero seed
uint8_t crc6(uint32_t data18) {
  uint8_t crc = 0;
  for (int bit = 17; bit >= 0; bit--) {
    const uint8_t in = ((data18 >> bit) & 1) ^ ((crc >> 5) & 1);
    crc = ((crc << 1) & 0x3F) ^ (in ? 0x03 : 0);
  }
  return crc;
}

// A frame as the chip sends it: 14 angle bits, 4 status bits, 6 CRC bits
std::array<uint8_t, 3> make_frame(uint16_t count, uint8_t status) {
  const uint32_t data18 = (static_cast<uint32_t>(count & 0x3FFF) << 4) | (status & 0x0F);
  const uint32_t frame = (data18 << 6) | crc6(data18);
  return {static_cast<uint8_t>(frame >> 16), static_cast<uint8_t>(frame >> 8), static_cast<uint8_t>(frame)};
}

}  // namespace

TEST(MT6701SpiFrame, DecodesANonZeroReading) {
  const auto frame = make_frame(0x1234, 0b0101);
  uint16_t count = 0;
  uint8_t status = 0;
  ASSERT_TRUE(decode_frame(frame.data(), count, status));
  EXPECT_EQ(count, 0x1234);
  EXPECT_EQ(status, 0b0101);
}

TEST(MT6701SpiFrame, AZeroReadingIsValid) {
  const auto frame = make_frame(0, 0);
  EXPECT_EQ(frame, (std::array<uint8_t, 3>{0, 0, 0}));
  uint16_t count = 1;
  uint8_t status = 1;
  ASSERT_TRUE(decode_frame(frame.data(), count, status));
  EXPECT_EQ(count, 0);
  EXPECT_EQ(status, 0);
}

TEST(MT6701SpiFrame, EveryAngleAndStatusRoundTrips) {
  for (uint32_t count = 0; count < 16384; count += 37) {
    for (uint8_t status = 0; status < 16; status++) {
      const auto frame = make_frame(count, status);
      uint16_t got_count = 0;
      uint8_t got_status = 0;
      ASSERT_TRUE(decode_frame(frame.data(), got_count, got_status)) << count << " " << status;
      EXPECT_EQ(got_count, count);
      EXPECT_EQ(got_status, status);
    }
  }
}

TEST(MT6701SpiFrame, ACorruptedBitFailsTheCrc) {
  const auto good = make_frame(0x2ACE, 0b1010);
  for (int bit = 0; bit < 24; bit++) {
    auto frame = good;
    frame[2 - bit / 8] ^= 1 << (bit % 8);
    uint16_t count = 0;
    uint8_t status = 0;
    EXPECT_FALSE(decode_frame(frame.data(), count, status)) << "bit " << bit;
  }
}

TEST(MT6701SpiFrame, StatusBitsAreTheFieldPushAndTrackFlags) {
  uint16_t count = 0;
  uint8_t status = 0;
  // Mg[1:0] = 2 (field too weak), Mg[2] push button, Mg[3] loss of track
  ASSERT_TRUE(decode_frame(make_frame(100, 0b1110).data(), count, status));
  EXPECT_EQ(status & 0x03, 2);
  EXPECT_TRUE(status & 0x04);
  EXPECT_TRUE(status & 0x08);
}

TEST(MT6701SpiProbe, ThreeGoodFramesProveTheEncoder) {
  EXPECT_EQ(probe_result(0, false), ProbeResult::PROBE_RESULT_NO_ENCODER);
  EXPECT_EQ(probe_result(PROBE_MIN_VALID - 1, true), ProbeResult::PROBE_RESULT_NO_ENCODER);
  EXPECT_EQ(probe_result(PROBE_MIN_VALID, true), ProbeResult::PROBE_RESULT_OK);
  EXPECT_EQ(probe_result(PROBE_FRAMES, true), ProbeResult::PROBE_RESULT_OK);
}

TEST(MT6701SpiProbe, AllZeroFramesOnlyWarn) {
  EXPECT_EQ(probe_result(PROBE_MIN_VALID, false), ProbeResult::PROBE_RESULT_ALL_ZERO);
  EXPECT_EQ(probe_result(PROBE_FRAMES, false), ProbeResult::PROBE_RESULT_ALL_ZERO);
}

}  // namespace esphome::mt6701_spi::testing
