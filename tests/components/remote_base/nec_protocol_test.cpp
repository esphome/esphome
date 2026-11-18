#include <gtest/gtest.h>
#include <vector>
#include "esphome/components/remote_base/nec_protocol.h"

namespace esphome::remote_base::testing {

// NEC timings from the spec (esphome/issues#6603), independent of the encoder constants
static constexpr int32_t HEADER_MARK = 9000;
static constexpr int32_t HEADER_SPACE = -4500;
static constexpr int32_t REPEAT_HEADER_SPACE = -2250;
static constexpr int32_t BIT_MARK = 560;
static constexpr int32_t ONE_SPACE = -1690;
static constexpr int32_t ZERO_SPACE = -560;
static constexpr int32_t REPEAT_PERIOD = 108000;
static constexpr int32_t REPEAT_GAP = -(REPEAT_PERIOD - (HEADER_MARK - REPEAT_HEADER_SPACE + BIT_MARK));

// Builds a full NEC frame by hand: header, address and command LSB first, stop mark
static std::vector<int32_t> build_frame(uint16_t address, uint16_t command) {
  std::vector<int32_t> raw{HEADER_MARK, HEADER_SPACE};
  for (uint16_t value : {address, command}) {
    for (int bit = 0; bit < 16; bit++) {
      raw.push_back(BIT_MARK);
      raw.push_back((value >> bit) & 1 ? ONE_SPACE : ZERO_SPACE);
    }
  }
  raw.push_back(BIT_MARK);
  return raw;
}

/// The space that puts the first repeat frame 108 ms after the start of this frame
static int32_t first_repeat_gap(const std::vector<int32_t> &frame) {
  int32_t duration = 0;
  for (int32_t item : frame) {
    duration += item < 0 ? -item : item;
  }
  return -(REPEAT_PERIOD - duration);
}

TEST(NECProtocolTest, EncodeWithoutRepeats) {
  NECProtocol protocol;
  RemoteTransmitData dst;
  protocol.encode(&dst, NECData{.address = 0x7F80, .command = 0xF20D, .command_repeats = 1});

  EXPECT_EQ(dst.get_carrier_frequency(), 38000u);
  EXPECT_EQ(dst.get_data(), build_frame(0x7F80, 0xF20D));
}

TEST(NECProtocolTest, EncodeWithRepeats) {
  NECProtocol protocol;
  RemoteTransmitData dst;
  protocol.encode(&dst, NECData{.address = 0x7F80, .command = 0xF20D, .command_repeats = 3});

  std::vector<int32_t> expected = build_frame(0x7F80, 0xF20D);
  expected.insert(expected.end(), {first_repeat_gap(expected), HEADER_MARK, REPEAT_HEADER_SPACE, BIT_MARK});
  expected.insert(expected.end(), {REPEAT_GAP, HEADER_MARK, REPEAT_HEADER_SPACE, BIT_MARK});
  EXPECT_EQ(dst.get_data(), expected);
}

TEST(NECProtocolTest, FirstRepeatGapFollowsFrameLength) {
  NECProtocol protocol;
  RemoteTransmitData all_zeros;
  protocol.encode(&all_zeros, NECData{.address = 0, .command = 0, .command_repeats = 2});
  RemoteTransmitData all_ones;
  protocol.encode(&all_ones, NECData{.address = 0xFFFF, .command = 0xFFFF, .command_repeats = 2});

  const std::vector<int32_t> zeros_frame = build_frame(0, 0);
  const std::vector<int32_t> ones_frame = build_frame(0xFFFF, 0xFFFF);
  EXPECT_EQ(all_zeros.get_data()[zeros_frame.size()], first_repeat_gap(zeros_frame));
  EXPECT_EQ(all_ones.get_data()[ones_frame.size()], first_repeat_gap(ones_frame));
  // Both frames start their repeat 108 ms after they began
  EXPECT_EQ(first_repeat_gap(zeros_frame) - first_repeat_gap(ones_frame),
            static_cast<int32_t>(32 * (ONE_SPACE - ZERO_SPACE)));
}

TEST(NECProtocolTest, EncodeClampsRepeats) {
  NECProtocol protocol;
  RemoteTransmitData none;
  protocol.encode(&none, NECData{.address = 0x1234, .command = 0x5678, .command_repeats = 0});
  EXPECT_EQ(none.get_data().size(), 67u);

  RemoteTransmitData many;
  protocol.encode(&many, NECData{.address = 0x1234, .command = 0x5678, .command_repeats = 500});
  EXPECT_EQ(many.get_data().size(), 67u + 4u * 9u);
}

TEST(NECProtocolTest, EncodeReserveMatchesSize) {
  NECProtocol protocol;
  RemoteTransmitData dst;
  protocol.encode(&dst, NECData{.address = 0x1234, .command = 0x5678, .command_repeats = 5});

  EXPECT_EQ(dst.get_data().size(), 67u + 4u * 4u);
  EXPECT_GE(dst.get_data().capacity(), dst.get_data().size());
}

TEST(NECProtocolTest, DecodeFullFrame) {
  NECProtocol protocol;
  std::vector<int32_t> raw = build_frame(0x1234, 0x5678);
  RemoteReceiveData rx(raw, 25, TOLERANCE_MODE_PERCENTAGE);
  auto decoded = protocol.decode(rx);

  ASSERT_TRUE(decoded.has_value());
  // clang-tidy's unchecked-optional-access does not model gtest's ASSERT_TRUE as a check
  if (decoded.has_value()) {
    EXPECT_EQ(decoded->address, 0x1234);
    EXPECT_EQ(decoded->command, 0x5678);
    EXPECT_EQ(decoded->command_repeats, 1);
  }
}

TEST(NECProtocolTest, DecodeWithoutStopMark) {
  NECProtocol protocol;
  std::vector<int32_t> raw = build_frame(0x1234, 0x5678);
  raw.pop_back();
  RemoteReceiveData rx(raw, 25, TOLERANCE_MODE_PERCENTAGE);
  auto decoded = protocol.decode(rx);

  ASSERT_TRUE(decoded.has_value());
  if (decoded.has_value()) {
    EXPECT_EQ(decoded->address, 0x1234);
    EXPECT_EQ(decoded->command, 0x5678);
  }
}

TEST(NECProtocolTest, DecodeStandaloneRepeatFrameRejected) {
  NECProtocol protocol;
  std::vector<int32_t> raw{HEADER_MARK, REPEAT_HEADER_SPACE, BIT_MARK};
  RemoteReceiveData rx(raw, 25, TOLERANCE_MODE_PERCENTAGE);

  EXPECT_FALSE(protocol.decode(rx).has_value());
}

}  // namespace esphome::remote_base::testing
