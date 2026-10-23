#include <gtest/gtest.h>
#include "esphome/components/remote_base/dooya_protocol.h"

namespace esphome::remote_base::testing {

namespace {

constexpr DooyaData DATA{.id = 0x12A5C3, .channel = 0x4E, .button = 0x9, .check = 0x6};

void add_bits(RawTimings &out, uint32_t value, uint8_t count) {
  for (uint32_t mask = 1UL << (count - 1); mask != 0; mask >>= 1) {
    const bool one = value & mask;
    out.push_back(one ? 750 : 350);
    out.push_back(one ? -350 : -750);
  }
}

// The frame as the encoder wrote it before it used item_bytes
RawTimings reference_frame(const DooyaData &data) {
  RawTimings out = {5000, -1500};
  add_bits(out, data.id, 24);
  add_bits(out, data.channel, 8);
  add_bits(out, data.button, 4);
  add_bits(out, data.check, 4);
  return out;
}

optional<DooyaData> decode(const RawTimings &timings) {
  return DooyaProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

}  // namespace

TEST(DooyaProtocolTest, EncodesTheSameFrame) {
  RemoteTransmitData tx;
  DooyaProtocol().encode(&tx, DATA);
  EXPECT_EQ(tx.get_data(), reference_frame(DATA));
}

TEST(DooyaProtocolTest, DecodesAFrameWithoutItsLastSpace) {
  for (const uint8_t check : {0x0, 0x1, 0x6, 0xF}) {
    DooyaData data = DATA;
    data.check = check;
    RawTimings timings = reference_frame(data);
    timings.pop_back();
    auto decoded = decode(timings);
    ASSERT_TRUE(decoded.has_value()) << int(check);
    EXPECT_EQ(decoded->id, data.id);
    EXPECT_EQ(decoded->channel, data.channel);
    EXPECT_EQ(decoded->button, data.button);
    EXPECT_EQ(decoded->check, data.check);
  }
}

TEST(DooyaProtocolTest, RejectsABadMark) {
  for (const size_t index : {size_t{10}, size_t{80}}) {  // a payload bit, then the last mark
    RawTimings timings = reference_frame(DATA);
    timings.pop_back();
    timings[index] = 550;
    EXPECT_FALSE(decode(timings).has_value()) << index;
  }
}

}  // namespace esphome::remote_base::testing
