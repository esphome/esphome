#include <gtest/gtest.h>
#include "esphome/components/remote_base/keeloq_protocol.h"

namespace esphome::remote_base::testing {

namespace {

constexpr uint32_t BIT_US = 380;

KeeloqData frame(uint16_t suffix, uint8_t suffix_bits) {
  KeeloqData data{};
  data.encrypted = 0xd19ef0a9;
  data.address = 0x116ea01;
  data.command = 0x08;
  data.suffix = suffix;
  data.suffix_bits = suffix_bits;
  return data;
}

RawTimings encode(const KeeloqData &data) {
  RemoteTransmitData tx;
  KeeloqProtocol().encode(&tx, data);
  return tx.get_data();
}

// A receiver starts on the first mark and merges the guard into the previous space.
RawTimings as_received(RawTimings timings, bool drop_tail = false) {
  if (!timings.empty() && timings.front() < 0) {
    timings.erase(timings.begin());
  }
  RawTimings merged;
  for (int32_t item : timings) {
    if (!merged.empty() && (merged.back() < 0) == (item < 0)) {
      merged.back() += item;
    } else {
      merged.push_back(item);
    }
  }
  if (drop_tail && !merged.empty() && merged.back() < 0) {
    merged.pop_back();
  }
  return merged;
}

optional<KeeloqData> decode(const RawTimings &timings) {
  return KeeloqProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

void expect_round_trip(uint16_t suffix, uint8_t suffix_bits, bool drop_tail = false) {
  auto decoded = decode(as_received(encode(frame(suffix, suffix_bits)), drop_tail));
  ASSERT_TRUE(decoded.has_value()) << "suffix 0x" << std::hex << suffix << "/" << std::dec << int(suffix_bits);
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(decoded->address, 0x116ea01u);
  EXPECT_EQ(decoded->command, 0x08);
  EXPECT_EQ(decoded->encrypted, 0xd19ef0a9u);
  EXPECT_TRUE(decoded->repeat);
  EXPECT_FALSE(decoded->vlow);
  EXPECT_EQ(decoded->suffix, suffix);
  EXPECT_EQ(decoded->suffix_bits, suffix_bits);
}

}  // namespace

TEST(KeeloqProtocolTest, PlainWordHasNoSuffix) { expect_round_trip(0, 0); }

TEST(KeeloqProtocolTest, SuffixRoundTripsWhenTheGuardSwallowsTheLastSpace) {
  expect_round_trip(0x00, 8);
  expect_round_trip(0x80, 8);
  expect_round_trip(0xA5, 8);
  expect_round_trip(0x0000, 16);
  expect_round_trip(0x8000, 16);
  expect_round_trip(0, 1);
  expect_round_trip(1, 1);
}

TEST(KeeloqProtocolTest, SuffixRoundTripsWhenTheCaptureEndsAtIdle) {
  expect_round_trip(0x00, 8, true);
  expect_round_trip(0x80, 8, true);
  expect_round_trip(0x8000, 16, true);
  expect_round_trip(1, 1, true);
}

TEST(KeeloqProtocolTest, ExtraBitsThatEndAtIdleStayASuffix) {
  RawTimings timings = encode(frame(0, 0));
  if (!timings.empty() && timings.front() < 0) {
    timings.erase(timings.begin());
  }
  ASSERT_LT(timings.back(), 0);
  timings.pop_back();
  for (uint8_t i = 0; i < 4; i++) {
    timings.push_back(static_cast<int32_t>(2 * BIT_US));
    timings.push_back(-static_cast<int32_t>(BIT_US));
  }

  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  if (decoded.has_value()) {
    EXPECT_EQ(decoded->suffix, 0);
    EXPECT_EQ(decoded->suffix_bits, 4);
  }
}

TEST(KeeloqProtocolTest, FollowingPwmIsNotASuffix) {
  // Drop only the guard item. The repeat bit keeps its own space, then more PWM follows.
  RawTimings timings = encode(frame(0, 0));
  if (!timings.empty() && timings.front() < 0) {
    timings.erase(timings.begin());
  }
  ASSERT_FALSE(timings.empty());
  ASSERT_LT(timings.back(), 0);
  timings.pop_back();
  for (uint8_t i = 0; i < 4; i++) {
    timings.push_back(static_cast<int32_t>(2 * BIT_US));
    timings.push_back(-static_cast<int32_t>(BIT_US));
  }
  timings.push_back(static_cast<int32_t>(3 * BIT_US));

  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  if (decoded.has_value()) {
    EXPECT_EQ(decoded->suffix, 0);
    EXPECT_EQ(decoded->suffix_bits, 0);
  }
}

}  // namespace esphome::remote_base::testing
