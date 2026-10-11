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
static constexpr int32_t FIRST_REPEAT_GAP = -40500;
static constexpr int32_t REPEAT_GAP = -96187;

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
  expected.insert(expected.end(), {FIRST_REPEAT_GAP, HEADER_MARK, REPEAT_HEADER_SPACE, BIT_MARK});
  expected.insert(expected.end(), {REPEAT_GAP, HEADER_MARK, REPEAT_HEADER_SPACE, BIT_MARK});
  EXPECT_EQ(dst.get_data(), expected);
}

TEST(NECProtocolTest, EncodeReserveMatchesSize) {
  NECProtocol protocol;
  RemoteTransmitData dst;
  protocol.encode(&dst, NECData{.address = 0x1234, .command = 0x5678, .command_repeats = 5});

  EXPECT_EQ(dst.get_data().size(), 67u + 4u * 4u);
  EXPECT_EQ(dst.get_data().capacity(), dst.get_data().size());
}

TEST(NECProtocolTest, DecodeFullFrame) {
  NECProtocol protocol;
  std::vector<int32_t> raw = build_frame(0x1234, 0x5678);
  RemoteReceiveData rx(raw, 25, TOLERANCE_MODE_PERCENTAGE);
  auto decoded = protocol.decode(rx);

  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->address, 0x1234);
  EXPECT_EQ(decoded->command, 0x5678);
  EXPECT_EQ(decoded->command_repeats, 1);
}

TEST(NECProtocolTest, DecodeWithoutStopMark) {
  NECProtocol protocol;
  std::vector<int32_t> raw = build_frame(0x1234, 0x5678);
  raw.pop_back();
  RemoteReceiveData rx(raw, 25, TOLERANCE_MODE_PERCENTAGE);
  auto decoded = protocol.decode(rx);

  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->address, 0x1234);
  EXPECT_EQ(decoded->command, 0x5678);
}

TEST(NECProtocolTest, DecodeStandaloneRepeatFrameRejected) {
  NECProtocol protocol;
  std::vector<int32_t> raw{HEADER_MARK, REPEAT_HEADER_SPACE, BIT_MARK};
  RemoteReceiveData rx(raw, 25, TOLERANCE_MODE_PERCENTAGE);

  EXPECT_FALSE(protocol.decode(rx).has_value());
}

}  // namespace esphome::remote_base::testing
