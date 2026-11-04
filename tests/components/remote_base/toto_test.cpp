#include <gtest/gtest.h>
#include "esphome/components/remote_base/toto_protocol.h"

namespace esphome::remote_base::testing {

namespace {

// "Full Flush" as captured from a Toto remote in IRremoteESP8266 issue 1806, first of its two frames
const RawTimings FULL_FLUSH = {6266, -2734, 598, -540, 598, -1626, 598, -512,  622, -516,  598, -514,  598, -510,
                               598,  -514,  628, -512, 596, -514,  600, -512,  598, -538,  600, -1622, 600, -512,
                               598,  -540,  602, -510, 598, -512,  598, -512,  624, -514,  598, -512,  598, -512,
                               598,  -514,  624, -512, 598, -514,  598, -1652, 596, -514,  598, -1626, 598, -1650,
                               598,  -514,  598, -512, 598, -540,  598, -514,  596, -1626, 626, -512,  574, -1648,
                               598,  -1650, 598, -514, 598, -512,  594, -544,  596, -514,  598};

// The frame as it was sent before the bit order changed: rc_code_1, rc_code_2, command and checksum, most
// significant bit first
RawTimings reference_frame(uint8_t rc_code_1, uint8_t rc_code_2, uint8_t command) {
  RawTimings out{6000, -3000};
  auto bits = [&out](uint32_t value, uint8_t count) {
    for (uint32_t mask = 1u << (count - 1); mask != 0; mask >>= 1) {
      out.push_back(600);
      out.push_back((value & mask) ? -1667 : -520);
    }
  };
  bits(0x2008, 15);
  const uint8_t rc_codes = rc_code_1 << 4 | rc_code_2;
  bits(rc_codes << 16 | command << 8 | (rc_codes ^ command), 24);
  out.push_back(600);
  return out;
}

optional<TotoData> decode(const RawTimings &timings) {
  return TotoProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

}  // namespace

TEST(TotoProtocolTest, DecodesARealCapture) {
  auto decoded = decode(FULL_FLUSH);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->code, 0x0D00);
}

TEST(TotoProtocolTest, FillsTheOldFieldsOnReceive) {
  RemoteTransmitData tx;
  TotoProtocol().encode(&tx, TotoData{.code = 0x37BB});
  auto decoded = decode(tx.get_data());
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->rc_code_1, 0x0D);
  EXPECT_EQ(decoded->rc_code_2, 0x0D);
  EXPECT_EQ(decoded->command, 0xEC);
}

TEST(TotoProtocolTest, SendsTheSameFrameAsTheOldFields) {
  RemoteTransmitData tx;
  TotoProtocol().encode(&tx, TotoData{.code = 0x37BB});
  EXPECT_EQ(tx.get_data(), reference_frame(0x0D, 0x0D, 0xEC));
}

TEST(TotoProtocolTest, RoundTrips) {
  for (const uint16_t code : {0x0000, 0x0134, 0x0234, 0x0D00, 0x37BB, 0xFFFF}) {
    RemoteTransmitData tx;
    TotoProtocol().encode(&tx, TotoData{.code = code});
    auto decoded = decode(tx.get_data());
    ASSERT_TRUE(decoded.has_value()) << code;
    EXPECT_EQ(decoded->code, code);
  }
}

TEST(TotoProtocolTest, RejectsABadChecksum) {
  RawTimings timings = FULL_FLUSH;
  // Flip the last payload bit, which is the top bit of the checksum
  timings[timings.size() - 2] = -1700;
  EXPECT_FALSE(decode(timings).has_value());
}

}  // namespace esphome::remote_base::testing
