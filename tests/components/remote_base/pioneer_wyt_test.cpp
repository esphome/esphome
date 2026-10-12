#include <gtest/gtest.h>
#include <array>
#include <vector>
#include "esphome/components/remote_base/pioneer_wyt_protocol.h"

namespace esphome::remote_base::testing {

namespace {

using Frame = std::array<uint8_t, WYT_REMOTE_COMMAND_SIZE>;

// Factory remote capture: cool mode, 26°C, beeper on, display on; the last byte is the checksum
constexpr Frame COOL_26_CAPTURE = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x24, 0x03, 0x05, 0x00, 0x00, 0x00, 0x00, 0x80, 0xC1};

Frame bytes(const PioneerWytData &data) {
  Frame out{};
  for (size_t i = 0; i < out.size(); i++)
    out[i] = data[i];
  return out;
}

PioneerWytData general(float target_temperature) {
  return PioneerWytData::make_general(true, PIONEER_WYT_MODE_COOL, target_temperature, true, true, false, false, false,
                                      false, 0, false, false);
}

}  // namespace

TEST(PioneerWytProtocolTest, GeneralMatchesTheCapture) { EXPECT_EQ(bytes(general(26.0f)), COOL_26_CAPTURE); }

TEST(PioneerWytProtocolTest, RawCodeGetsTheCaptureChecksum) {
  std::vector<uint8_t> code(COOL_26_CAPTURE.begin(), COOL_26_CAPTURE.end() - 1);
  PioneerWytData data(code);
  data.finalize();
  EXPECT_EQ(bytes(data), COOL_26_CAPTURE);
}

// FAN frames add 0x0F to the byte sum
TEST(PioneerWytProtocolTest, FanFrame) {
  constexpr Frame expected = {0x23, 0xCB, 0x26, 0x02, 0x00, 0x40, 0xC0, 0x10, 0xC0, 0x00, 0x00, 0x00, 0x00, 0xF5};
  EXPECT_EQ(bytes(PioneerWytData::make_fan(PIONEER_WYT_FAN_HIGH, false, true, false)), expected);
}

TEST(PioneerWytProtocolTest, HalfDegreeSetsTheHalfBit) {
  auto data = general(22.5f);
  EXPECT_EQ(data[7], 31 - 22);
  EXPECT_EQ(data[12], 0x84);
}

// A lambda can bypass the schema range, so out of range temperatures clamp to 16..31
TEST(PioneerWytProtocolTest, TemperatureIsClamped) {
  EXPECT_EQ(general(40.0f)[7], 0);
  EXPECT_EQ(general(31.5f)[12], 0x80);
  EXPECT_EQ(general(-5.0f)[7], 31 - 16);
  EXPECT_EQ(general(0.0f)[7], 31 - 16);
}

TEST(PioneerWytProtocolTest, EncodesHeaderAndAllBits) {
  RemoteTransmitData dst;
  PioneerWytProtocol().encode(&dst, general(26.0f));
  // header mark, space, bit mark, then a space and mark per bit
  EXPECT_EQ(dst.get_data().size(), 3u + WYT_REMOTE_COMMAND_SIZE * 8 * 2);
  EXPECT_EQ(dst.get_carrier_frequency(), 38000u);
}

}  // namespace esphome::remote_base::testing
