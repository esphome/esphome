#include <gtest/gtest.h>
#include <array>
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::remote_base::testing {

namespace {

constexpr uint32_t BIT_MARK = 500;
constexpr uint32_t ONE_SPACE = 1500;
constexpr uint32_t ZERO_SPACE = 400;
constexpr std::array<uint8_t, 4> FRAME = {0xA5, 0x01, 0x80, 0xFF};

RemoteReceiveData receive(const RawTimings &timings) {
  return RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE);
}

RawTimings expected_items(uint8_t byte, bool msb_first) {
  RawTimings out;
  for (uint8_t bit = 0; bit < 8; bit++) {
    const uint8_t mask = msb_first ? (0x80 >> bit) : (1 << bit);
    out.push_back(BIT_MARK);
    out.push_back(-static_cast<int32_t>((byte & mask) ? ONE_SPACE : ZERO_SPACE));
  }
  return out;
}

}  // namespace

TEST(RemoteBaseByteItemsTest, LsbItemSequence) {
  RemoteTransmitData tx;
  const uint8_t byte = 0x4B;
  tx.item_bytes_lsb(&byte, 1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
  EXPECT_EQ(tx.get_data(), expected_items(byte, false));
}

TEST(RemoteBaseByteItemsTest, MsbItemSequence) {
  RemoteTransmitData tx;
  const uint8_t byte = 0x4B;
  tx.item_bytes_msb(&byte, 1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
  EXPECT_EQ(tx.get_data(), expected_items(byte, true));
}

TEST(RemoteBaseByteItemsTest, AppendsAfterExistingItems) {
  RemoteTransmitData tx;
  tx.item(9000, 4500);
  tx.item_bytes_lsb(FRAME.data(), FRAME.size(), BIT_MARK, ONE_SPACE, ZERO_SPACE);
  tx.mark(BIT_MARK);
  ASSERT_EQ(tx.get_data().size(), 2 + FRAME.size() * 16 + 1);
  EXPECT_EQ(tx.get_data()[0], 9000);
  EXPECT_EQ(tx.get_data()[1], -4500);
  EXPECT_EQ(tx.get_data().back(), static_cast<int32_t>(BIT_MARK));
}

TEST(RemoteBaseByteItemsTest, LsbRoundTrip) {
  RemoteTransmitData tx;
  tx.item_bytes_lsb(FRAME.data(), FRAME.size(), BIT_MARK, ONE_SPACE, ZERO_SPACE);
  auto rx = receive(tx.get_data());
  std::array<uint8_t, FRAME.size()> out{};
  ASSERT_TRUE(rx.expect_bytes_lsb(out.data(), out.size(), BIT_MARK, ONE_SPACE, ZERO_SPACE));
  EXPECT_EQ(out, FRAME);
  EXPECT_FALSE(rx.is_valid());
}

TEST(RemoteBaseByteItemsTest, MsbRoundTrip) {
  RemoteTransmitData tx;
  tx.item_bytes_msb(FRAME.data(), FRAME.size(), BIT_MARK, ONE_SPACE, ZERO_SPACE);
  auto rx = receive(tx.get_data());
  std::array<uint8_t, FRAME.size()> out{};
  ASSERT_TRUE(rx.expect_bytes_msb(out.data(), out.size(), BIT_MARK, ONE_SPACE, ZERO_SPACE));
  EXPECT_EQ(out, FRAME);
}

TEST(RemoteBaseByteItemsTest, BitOrderMismatchReversesBits) {
  RemoteTransmitData tx;
  const uint8_t byte = 0x01;
  tx.item_bytes_lsb(&byte, 1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
  auto rx = receive(tx.get_data());
  uint8_t out = 0;
  ASSERT_TRUE(rx.expect_bytes_msb(&out, 1, BIT_MARK, ONE_SPACE, ZERO_SPACE));
  EXPECT_EQ(out, 0x80);
}

TEST(RemoteBaseByteItemsTest, FailsMidByteAndStopsThere) {
  RemoteTransmitData tx;
  tx.item_bytes_lsb(FRAME.data(), FRAME.size(), BIT_MARK, ONE_SPACE, ZERO_SPACE);
  RawTimings timings = tx.get_data();
  // Bit 3 of byte 1 gets a space that matches neither length
  const size_t bad = (8 + 3) * 2 + 1;
  timings[bad] = -3000;
  auto rx = receive(timings);
  std::array<uint8_t, FRAME.size()> out = {0x11, 0x22, 0x33, 0x44};
  EXPECT_FALSE(rx.expect_bytes_lsb(out.data(), out.size(), BIT_MARK, ONE_SPACE, ZERO_SPACE));
  EXPECT_EQ(rx.get_index(), bad - 1);
  // Byte 0 was read in full, the partial byte and the rest are untouched
  EXPECT_EQ(out[0], FRAME[0]);
  EXPECT_EQ(out[1], 0x22);
  EXPECT_EQ(out[2], 0x33);
}

TEST(RemoteBaseByteItemsTest, FailsWhenDataRunsOut) {
  RemoteTransmitData tx;
  tx.item_bytes_msb(FRAME.data(), 1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
  auto rx = receive(tx.get_data());
  std::array<uint8_t, 2> out{};
  EXPECT_FALSE(rx.expect_bytes_msb(out.data(), out.size(), BIT_MARK, ONE_SPACE, ZERO_SPACE));
  EXPECT_EQ(out[0], FRAME[0]);
}

TEST(RemoteBaseByteItemsTest, ZeroLength) {
  RemoteTransmitData tx;
  tx.item_bytes_lsb(nullptr, 0, BIT_MARK, ONE_SPACE, ZERO_SPACE);
  tx.item_bytes_msb(nullptr, 0, BIT_MARK, ONE_SPACE, ZERO_SPACE);
  EXPECT_TRUE(tx.get_data().empty());

  const RawTimings empty;
  auto rx = receive(empty);
  EXPECT_TRUE(rx.expect_bytes_lsb(nullptr, 0, BIT_MARK, ONE_SPACE, ZERO_SPACE));
  EXPECT_TRUE(rx.expect_bytes_msb(nullptr, 0, BIT_MARK, ONE_SPACE, ZERO_SPACE));
  EXPECT_EQ(rx.get_index(), 0u);
}

}  // namespace esphome::remote_base::testing
