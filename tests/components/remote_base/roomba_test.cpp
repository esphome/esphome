#include <gtest/gtest.h>
#include "esphome/components/remote_base/roomba_protocol.h"

namespace esphome::remote_base::testing {

namespace {

// The frame as the encoder wrote it before it used item_bytes: eight bits, most significant first
RawTimings reference_frame(uint8_t value) {
  RawTimings out;
  for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
    const bool one = value & mask;
    out.push_back(one ? 3000 : 1000);
    out.push_back(one ? -1000 : -3000);
  }
  return out;
}

optional<RoombaData> decode(const RawTimings &timings) {
  return RoombaProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

}  // namespace

TEST(RoombaProtocolTest, EncodesTheSameFrame) {
  RemoteTransmitData tx;
  RoombaProtocol().encode(&tx, RoombaData{.data = 0xA5});
  EXPECT_EQ(tx.get_data(), reference_frame(0xA5));
}

TEST(RoombaProtocolTest, DecodesAFrameWithoutItsLastSpace) {
  for (const uint8_t value : {0x00, 0x01, 0x88, 0xA5, 0xFF}) {
    RawTimings timings = reference_frame(value);
    timings.pop_back();
    auto decoded = decode(timings);
    ASSERT_TRUE(decoded.has_value()) << int(value);
    if (!decoded.has_value()) {
      return;
    }
    EXPECT_EQ(decoded->data, value);
  }
}

TEST(RoombaProtocolTest, RejectsABadMark) {
  RawTimings timings = reference_frame(0xA5);
  timings.pop_back();
  timings[4] = 2000;
  EXPECT_FALSE(decode(timings).has_value());
}

}  // namespace esphome::remote_base::testing
