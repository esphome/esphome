#include <gtest/gtest.h>
#include <array>
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::remote_base::testing {

namespace {

constexpr uint32_t BIT_MARK = 500;
constexpr uint32_t ONE_SPACE = 1500;
constexpr uint32_t ZERO_SPACE = 400;
constexpr BitTimings TIMINGS{.bit_mark = BIT_MARK, .one_space = ONE_SPACE, .zero_space = ZERO_SPACE};
constexpr BitTimings MSB_TIMINGS{
    .bit_mark = BIT_MARK, .one_space = ONE_SPACE, .zero_space = ZERO_SPACE, .bit_order = BitOrder::BIT_ORDER_MSB_FIRST};
constexpr std::array<uint8_t, 4> FRAME = {0xA5, 0x01, 0x80, 0xFF};
// Mark length carries the bit (one long mark, zero short), as in roomba
constexpr BitTimings MARK_TIMINGS{.one_space = 1000, .zero_space = 3000, .one_mark = 3000, .zero_mark = 1000};
// Space first and both lengths vary, as in Cardin S438
constexpr BitTimings SPACE_FIRST_TIMINGS{
    .one_space = 750, .zero_space = 1460, .one_mark = 1460, .zero_mark = 750, .space_first = true};

static_assert(TIMINGS.one_mark == BIT_MARK && TIMINGS.zero_mark == BIT_MARK);

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
  tx.item_bytes<TIMINGS>(&byte, 1);
  EXPECT_EQ(tx.get_data(), expected_items(byte, false));
}

TEST(RemoteBaseByteItemsTest, MsbItemSequence) {
  RemoteTransmitData tx;
  const uint8_t byte = 0x4B;
  tx.item_bytes<MSB_TIMINGS>(&byte, 1);
  EXPECT_EQ(tx.get_data(), expected_items(byte, true));
}

TEST(RemoteBaseByteItemsTest, AppendsAfterExistingItems) {
  RemoteTransmitData tx;
  tx.item(9000, 4500);
  tx.item_bytes<TIMINGS>(FRAME.data(), FRAME.size());
  tx.mark(BIT_MARK);
  ASSERT_EQ(tx.get_data().size(), 2 + FRAME.size() * 16 + 1);
  EXPECT_EQ(tx.get_data()[0], 9000);
  EXPECT_EQ(tx.get_data()[1], -4500);
  EXPECT_EQ(tx.get_data().back(), static_cast<int32_t>(BIT_MARK));
}

TEST(RemoteBaseByteItemsTest, LsbRoundTrip) {
  RemoteTransmitData tx;
  tx.item_bytes<TIMINGS>(FRAME.data(), FRAME.size());
  auto rx = receive(tx.get_data());
  std::array<uint8_t, FRAME.size()> out{};
  ASSERT_TRUE(rx.expect_bytes<TIMINGS>(out.data(), out.size()));
  EXPECT_EQ(out, FRAME);
  EXPECT_FALSE(rx.is_valid());
}

TEST(RemoteBaseByteItemsTest, MsbRoundTrip) {
  RemoteTransmitData tx;
  tx.item_bytes<MSB_TIMINGS>(FRAME.data(), FRAME.size());
  auto rx = receive(tx.get_data());
  std::array<uint8_t, FRAME.size()> out{};
  ASSERT_TRUE(rx.expect_bytes<MSB_TIMINGS>(out.data(), out.size()));
  EXPECT_EQ(out, FRAME);
}

TEST(RemoteBaseByteItemsTest, BitOrderMismatchReversesBits) {
  RemoteTransmitData tx;
  const uint8_t byte = 0x01;
  tx.item_bytes<TIMINGS>(&byte, 1);
  auto rx = receive(tx.get_data());
  uint8_t out = 0;
  ASSERT_TRUE(rx.expect_bytes<MSB_TIMINGS>(&out, 1));
  EXPECT_EQ(out, 0x80);
}

TEST(RemoteBaseByteItemsTest, FailsMidByteAndStopsThere) {
  RemoteTransmitData tx;
  tx.item_bytes<TIMINGS>(FRAME.data(), FRAME.size());
  RawTimings timings = tx.get_data();
  // Bit 3 of byte 1 gets a space that matches neither length
  const size_t bad = (8 + 3) * 2 + 1;
  timings[bad] = -3000;
  auto rx = receive(timings);
  std::array<uint8_t, FRAME.size()> out = {0x11, 0x22, 0x33, 0x44};
  EXPECT_FALSE(rx.expect_bytes<TIMINGS>(out.data(), out.size()));
  EXPECT_EQ(rx.get_index(), bad - 1);
  // Byte 0 was read in full, the partial byte and the rest are untouched
  EXPECT_EQ(out[0], FRAME[0]);
  EXPECT_EQ(out[1], 0x22);
  EXPECT_EQ(out[2], 0x33);
}

TEST(RemoteBaseByteItemsTest, FailsWhenDataRunsOut) {
  RemoteTransmitData tx;
  tx.item_bytes<MSB_TIMINGS>(FRAME.data(), 1);
  auto rx = receive(tx.get_data());
  std::array<uint8_t, 2> out{};
  EXPECT_FALSE(rx.expect_bytes<MSB_TIMINGS>(out.data(), out.size()));
  EXPECT_EQ(out[0], FRAME[0]);
}

TEST(RemoteBaseByteItemsTest, ZeroLength) {
  RemoteTransmitData tx;
  tx.item_bytes<TIMINGS>(nullptr, 0);
  tx.item_bytes<MSB_TIMINGS>(nullptr, 0);
  EXPECT_TRUE(tx.get_data().empty());

  const RawTimings empty;
  auto rx = receive(empty);
  EXPECT_TRUE(rx.expect_bytes<TIMINGS>(nullptr, 0));
  EXPECT_TRUE(rx.expect_bytes<MSB_TIMINGS>(nullptr, 0));
  EXPECT_EQ(rx.get_index(), 0u);
}

TEST(RemoteBaseByteItemsTest, PerBitMarkSequence) {
  RemoteTransmitData tx;
  const uint8_t byte = 0x01;
  tx.item_bytes<MARK_TIMINGS>(&byte, 1);
  const RawTimings &data = tx.get_data();
  ASSERT_EQ(data.size(), 16u);
  EXPECT_EQ(data[0], 3000);
  EXPECT_EQ(data[1], -1000);
  EXPECT_EQ(data[2], 1000);
  EXPECT_EQ(data[3], -3000);
}

TEST(RemoteBaseByteItemsTest, SpaceFirstSequence) {
  RemoteTransmitData tx;
  const uint8_t byte = 0x01;
  tx.item_bytes<SPACE_FIRST_TIMINGS>(&byte, 1);
  const RawTimings &data = tx.get_data();
  ASSERT_EQ(data.size(), 16u);
  EXPECT_EQ(data[0], -750);
  EXPECT_EQ(data[1], 1460);
  EXPECT_EQ(data[2], -1460);
  EXPECT_EQ(data[3], 750);
}

TEST(RemoteBaseByteItemsTest, PerBitMarkRoundTrip) {
  RemoteTransmitData tx;
  tx.item_bytes<MARK_TIMINGS>(FRAME.data(), FRAME.size());
  auto rx = receive(tx.get_data());
  std::array<uint8_t, 4> out{};
  ASSERT_TRUE(rx.expect_bytes<MARK_TIMINGS>(out.data(), out.size()));
  EXPECT_EQ(out, FRAME);
}

TEST(RemoteBaseByteItemsTest, SpaceFirstRoundTrip) {
  RemoteTransmitData tx;
  tx.item_bytes<SPACE_FIRST_TIMINGS>(FRAME.data(), FRAME.size());
  auto rx = receive(tx.get_data());
  std::array<uint8_t, 4> out{};
  ASSERT_TRUE(rx.expect_bytes<SPACE_FIRST_TIMINGS>(out.data(), out.size()));
  EXPECT_EQ(out, FRAME);
  EXPECT_EQ(rx.get_index(), tx.get_data().size());
}

TEST(RemoteBaseByteItemsTest, SpaceFirstLeavesABitWithTheWrongMarkUnread) {
  // A one's space followed by a zero's mark is neither bit
  const RawTimings timings = {-750, 750};
  auto rx = receive(timings);
  uint8_t out = 0;
  EXPECT_FALSE(rx.expect_bytes<SPACE_FIRST_TIMINGS>(&out, 1));
  EXPECT_EQ(rx.get_index(), 0u);
}

}  // namespace esphome::remote_base::testing
