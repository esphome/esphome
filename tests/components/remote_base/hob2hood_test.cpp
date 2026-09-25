#include <gtest/gtest.h>
#include "esphome/components/remote_base/hob2hood_protocol.h"

namespace esphome::remote_base::testing {

namespace {

constexpr std::array<Hob2HoodCommand, 7> ALL_COMMANDS = {
    HOB2HOOD_COMMAND_LIGHT_OFF,  HOB2HOOD_COMMAND_LIGHT_ON, HOB2HOOD_COMMAND_FAN_OFF, HOB2HOOD_COMMAND_FAN_LOW,
    HOB2HOOD_COMMAND_FAN_MEDIUM, HOB2HOOD_COMMAND_FAN_HIGH, HOB2HOOD_COMMAND_FAN_MAX,
};

RawTimings encode(Hob2HoodCommand command) {
  RemoteTransmitData data;
  Hob2HoodProtocol().encode(&data, Hob2HoodData{command});
  return data.get_data();
}

optional<Hob2HoodData> decode(const RawTimings &timings, uint32_t tolerance = 25,
                              ToleranceMode mode = TOLERANCE_MODE_PERCENTAGE) {
  return Hob2HoodProtocol().decode(RemoteReceiveData(timings, tolerance, mode));
}

}  // namespace

// light_on is 0xd2: the bits 0 11010010 11010011 11010100 form 17 runs. A run of n zero bits is a mark of
// n * 700 + 300 us; a run of n one bits is a space of n * 700 - 200 us.
TEST(Hob2HoodProtocolTest, EncodesTheDocumentedTimings) {
  const RawTimings expected = {1000, -1200, 1000,  -500, 1700, -500, 1000, -1200, 1000,
                               -500, 1700,  -2600, 1000, -500, 1000, -500, 1700};
  EXPECT_EQ(encode(HOB2HOOD_COMMAND_LIGHT_ON), expected);
}

TEST(Hob2HoodProtocolTest, LongestFrameFitsTheReservedLength) {
  for (auto command : ALL_COMMANDS) {
    EXPECT_LE(encode(command).size(), 18u) << "command 0x" << std::hex << int(command);
  }
  EXPECT_EQ(encode(HOB2HOOD_COMMAND_LIGHT_OFF).size(), 18u);
}

TEST(Hob2HoodProtocolTest, RoundTripsEveryCommand) {
  for (auto command : ALL_COMMANDS) {
    auto decoded = decode(encode(command));
    ASSERT_TRUE(decoded.has_value()) << "command 0x" << std::hex << int(command);
    EXPECT_EQ(decoded->command, command);
  }
}

// A receiver never captures the trailing space, so a frame that ends in one must still decode without it.
TEST(Hob2HoodProtocolTest, DecodesWithoutTheTrailingSpace) {
  for (auto command : ALL_COMMANDS) {
    auto timings = encode(command);
    if (timings.back() < 0)
      timings.pop_back();
    auto decoded = decode(timings);
    ASSERT_TRUE(decoded.has_value()) << "command 0x" << std::hex << int(command);
    EXPECT_EQ(decoded->command, command);
  }
}

// Real receivers shorten marks and lengthen spaces by a couple of hundred microseconds, which is why the
// documentation recommends a 350us tolerance.
TEST(Hob2HoodProtocolTest, DecodesSkewedTimingsWithinTheRecommendedTolerance) {
  auto timings = encode(HOB2HOOD_COMMAND_FAN_HIGH);
  for (auto &t : timings) {
    t += t > 0 ? -240 : -230;
  }
  auto decoded = decode(timings, 350, TOLERANCE_MODE_TIME);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->command, HOB2HOOD_COMMAND_FAN_HIGH);
}

TEST(Hob2HoodProtocolTest, RejectsAForeignFrame) {
  const RawTimings nec_like = {9000, -4500, 560, -560, 560, -1690, 560, -560, 560};
  EXPECT_FALSE(decode(nec_like).has_value());
  EXPECT_FALSE(decode({}).has_value());
}

}  // namespace esphome::remote_base::testing
