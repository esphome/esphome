#include <gtest/gtest.h>

#include "esphome/components/remote_base/aok_protocol.h"

namespace esphome::remote_base::testing {

namespace {

constexpr uint64_t FRAME = 0xA312345600A10B48ULL;
// The start mark and space, then 64 bits of a mark and a space each, then the closing mark and space
constexpr size_t TIMINGS_SIZE = (1 + 64 + 1) * 2;

RawTimings encode(const AOKData &data) {
  RemoteTransmitData tx;
  AOKProtocol().encode(&tx, data);
  return tx.get_data();
}

optional<AOKData> decode(const RawTimings &timings) {
  return AOKProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

// Bit `n` counted from the first one sent starts at timing 2 + 2 * n
RawTimings frame_timings(uint64_t frame) {
  RawTimings timings{5000, -600};
  for (int i = 63; i >= 0; i--) {
    if ((frame >> i) & 1) {
      timings.insert(timings.end(), {600, -285});
    } else {
      timings.insert(timings.end(), {285, -600});
    }
  }
  timings.insert(timings.end(), {600, -5000});
  return timings;
}

}  // namespace

TEST(AOKProtocolTest, EncodesKnownFrame) {
  EXPECT_EQ(encode(AOKData{.remote_id = 0x123456, .channels = 0x00A1, .command = 0x0B}), frame_timings(FRAME));
}

TEST(AOKProtocolTest, DecodesKnownFrame) {
  auto decoded = decode(frame_timings(FRAME));
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(decoded->remote_id, 0x123456u);
  EXPECT_EQ(decoded->channels, 0x00A1);
  EXPECT_EQ(decoded->command, 0x0B);
}

TEST(AOKProtocolTest, RoundTrips) {
  for (const AOKData &sent :
       {AOKData{0x000000, 0x0000, 0x00}, AOKData{0xFFFFFF, 0xFFFF, 0xFF}, AOKData{0xABCDEF, 0x8001, 0x7E}}) {
    const RawTimings timings = encode(sent);
    ASSERT_EQ(timings.size(), TIMINGS_SIZE);
    auto decoded = decode(timings);
    ASSERT_TRUE(decoded.has_value());
    if (!decoded.has_value()) {
      return;
    }
    EXPECT_TRUE(*decoded == sent);
  }
}

TEST(AOKProtocolTest, EncodingKeepsRemoteIdToItsFieldWidth) {
  auto decoded = decode(encode(AOKData{.remote_id = 0xFF123456, .channels = 0x00A1, .command = 0x0B}));
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(decoded->remote_id, 0x123456u);
}

TEST(AOKProtocolTest, RejectsBadChecksum) { EXPECT_FALSE(decode(frame_timings(FRAME ^ 1)).has_value()); }

TEST(AOKProtocolTest, RejectsBadHeader) {
  // The header is not part of the checksum, so only the header check can reject this
  EXPECT_FALSE(decode(frame_timings(FRAME ^ (1ULL << 56))).has_value());
}

TEST(AOKProtocolTest, SkipsLeadingNoise) {
  RawTimings timings{300, -1200, 150, -90};
  const RawTimings frame = frame_timings(FRAME);
  timings.insert(timings.end(), frame.begin(), frame.end());
  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(decoded->remote_id, 0x123456u);
  EXPECT_EQ(decoded->command, 0x0B);
}

TEST(AOKProtocolTest, UsesRepeatWhenFirstFrameIsCorrupt) {
  RawTimings timings = frame_timings(FRAME ^ 1);
  const RawTimings good = frame_timings(FRAME);
  timings.insert(timings.end(), good.begin(), good.end());
  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(decoded->command, 0x0B);
}

TEST(AOKProtocolTest, UsesRepeatWhenFirstFrameIsCutShort) {
  RawTimings timings = frame_timings(FRAME);
  timings.resize(timings.size() - 40);
  const RawTimings good = frame_timings(FRAME);
  timings.insert(timings.end(), good.begin(), good.end());
  EXPECT_TRUE(decode(timings).has_value());
}

TEST(AOKProtocolTest, RejectsBadStart) {
  RawTimings timings = frame_timings(FRAME);
  timings[0] = 2000;
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(AOKProtocolTest, RejectsTruncatedFrame) {
  RawTimings timings = frame_timings(FRAME);
  timings.resize(timings.size() - 6);
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(AOKProtocolTest, RejectsBadBitTiming) {
  RawTimings timings = frame_timings(FRAME);
  timings[2 + 2 * 20] = 1200;
  EXPECT_FALSE(decode(timings).has_value());
}

}  // namespace esphome::remote_base::testing
