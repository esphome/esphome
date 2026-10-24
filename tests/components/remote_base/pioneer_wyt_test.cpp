#include <gtest/gtest.h>

#include <array>
#include <vector>
#include "esphome/components/remote_base/pioneer_wyt_protocol.h"

namespace esphome::remote_base::testing {

namespace {

using Frame = std::array<uint8_t, PioneerWytData::SIZE>;

// Factory remote capture: cool mode, 26°C, beeper on, display on; the last byte is the checksum
constexpr Frame COOL_26_CAPTURE = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x24, 0x03, 0x05, 0x00, 0x00, 0x00, 0x00, 0x80, 0xC1};

Frame bytes(const PioneerWytData &data) {
  Frame out{};
  std::copy_n(data.data(), out.size(), out.begin());
  return out;
}

}  // namespace

TEST(PioneerWytProtocolTest, RawCodeGetsTheCaptureChecksum) {
  PioneerWytData data(std::vector<uint8_t>(COOL_26_CAPTURE.begin(), COOL_26_CAPTURE.end() - 1));
  data.finalize();
  EXPECT_EQ(bytes(data), COOL_26_CAPTURE);
}

TEST(PioneerWytProtocolTest, GivenChecksumIsRecomputed) {
  PioneerWytData data(COOL_26_CAPTURE.data(), COOL_26_CAPTURE.size());
  data[PioneerWytData::SIZE - 1] = 0x00;
  data.finalize();
  EXPECT_EQ(bytes(data), COOL_26_CAPTURE);
}

// FAN frames add 0x0F to the byte sum
TEST(PioneerWytProtocolTest, FanFrameChecksumHasOffset) {
  PioneerWytData data{0x23, 0xCB, 0x26, 0x02, 0x00, 0x40, 0xC0, 0x10, 0xC0, 0x00, 0x00, 0x00, 0x00};
  EXPECT_EQ(data.type(), PioneerWytData::PIONEER_WYT_TYPE_FAN);
  data.finalize();
  EXPECT_EQ(data[PioneerWytData::SIZE - 1], 0xF5);
}

TEST(PioneerWytProtocolTest, EncodesHeaderAndAllBits) {
  RemoteTransmitData dst;
  PioneerWytProtocol().encode(&dst, PioneerWytData(COOL_26_CAPTURE.data(), COOL_26_CAPTURE.size()));
  // header mark and space, a mark and space per bit, then the trailing mark
  const auto &raw = dst.get_data();
  ASSERT_EQ(raw.size(), 3u + PioneerWytData::SIZE * 8 * 2);
  EXPECT_EQ(dst.get_carrier_frequency(), 38000u);
  EXPECT_EQ(raw[0], 3100);
  EXPECT_EQ(raw[1], -1650);
  // 0x23 LSB first: bit 0 is one, bit 2 is zero
  EXPECT_EQ(raw[3], -1100);
  EXPECT_EQ(raw[7], -350);
  EXPECT_EQ(raw.back(), 500);
}

}  // namespace esphome::remote_base::testing
