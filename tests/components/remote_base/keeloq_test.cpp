#include <gtest/gtest.h>
#include <vector>
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

// A Jarolift frame as other implementations send it: 1150 us of carrier joined to the first of 13
// preamble marks, a 3880 us header, then 72 bits with 400/800 us marks. Values from a published test
// vector: code 0xF94A09F2, address 0x1234500, command 0x8, group 0x0F.
RawTimings jarolift_emulator_frame() {
  RawTimings timings{1150 + 400, -380};
  for (uint8_t i = 0; i < 12; i++) {
    timings.push_back(400);
    timings.push_back(-380);
  }
  timings.back() = -3880;
  const uint64_t word = (uint64_t{0x8} << 60) | (uint64_t{0x1234500} << 32) | 0xF94A09F2;
  const uint8_t group = 0x0F;
  for (uint8_t i = 0; i < 72; i++) {
    const bool one = i < 64 ? (word >> i) & 1 : (group >> (i - 64)) & 1;
    timings.push_back(one ? 400 : 800);
    timings.push_back(one ? -800 : -400);
  }
  timings.pop_back();  // the last space is the gap before the next frame
  return timings;
}

// Jarolift TDEF group UP for the channel mask 0x0F0F, from a published test vector. The mask's low byte is
// inside the hopping code, the high byte is the group sent after the 64-bit word.
KeeloqData tdef_frame(uint8_t group) {
  KeeloqData data{};
  data.encrypted = 0xF94A09F2;
  data.address = 0x1234500;
  data.command = 0x8;
  data.set_group(group);
  return data;
}

// The data bits after the 10-bit-time header: a 1-bit-time mark is a 1, a 2-bit-time mark a 0.
std::vector<bool> sent_bits(const RawTimings &timings) {
  std::vector<bool> bits;
  size_t i = 0;
  while (i < timings.size() && timings[i] != -static_cast<int32_t>(10 * BIT_US)) {
    i++;
  }
  for (i++; i < timings.size(); i++) {
    if (timings[i] > 0) {
      bits.push_back(timings[i] == static_cast<int32_t>(BIT_US));
    }
  }
  return bits;
}

uint32_t field(const std::vector<bool> &bits, size_t first, size_t count) {
  uint32_t value = 0;
  for (size_t i = 0; i < count; i++) {
    value |= static_cast<uint32_t>(bits[first + i]) << i;
  }
  return value;
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

// 0xA5 sends 1, 0, 1 first. Replace the space of one of those bits while the other bits still follow.
optional<KeeloqData> decode_with_suffix_space(uint8_t bit, uint32_t space_us) {
  RawTimings timings = as_received(encode(frame(0xA5, 8)));
  timings[timings.size() - 2 * 8 + 2 * bit + 1] = -static_cast<int32_t>(space_us);
  return decode(timings);
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

TEST(KeeloqProtocolTest, ASpaceThatIsNeitherDataNorAGapDropsTheSuffix) {
  // A 1 bit with the space of a 0 bit, and a 0 bit with the space of a 1 bit.
  for (const auto &[bit, space_us] : {std::pair<uint8_t, uint32_t>{2, BIT_US}, {1, 2 * BIT_US}}) {
    auto decoded = decode_with_suffix_space(bit, space_us);
    ASSERT_TRUE(decoded.has_value()) << "bit " << int(bit);
    if (decoded.has_value()) {
      EXPECT_EQ(decoded->address, 0x116ea01u);
      EXPECT_EQ(decoded->suffix, 0) << "bit " << int(bit);
      EXPECT_EQ(decoded->suffix_bits, 0) << "bit " << int(bit);
    }
  }
}

TEST(KeeloqProtocolTest, ASuffixFollowsARepeatBitOfZero) {
  // frame() sends the repeat bit as 1. Turn it into a 0 bit, with and without a suffix.
  for (uint8_t suffix_bits : {0, 8}) {
    RawTimings timings = as_received(encode(frame(suffix_bits > 0 ? 0xA5 : 0, suffix_bits)));
    const size_t repeat = timings.size() - 2 * suffix_bits - 2;
    timings[repeat] = static_cast<int32_t>(2 * BIT_US);
    if (suffix_bits > 0) {
      timings[repeat + 1] = -static_cast<int32_t>(BIT_US);
    }
    auto decoded = decode(timings);
    ASSERT_TRUE(decoded.has_value()) << int(suffix_bits);
    if (decoded.has_value()) {
      EXPECT_FALSE(decoded->repeat);
      EXPECT_EQ(decoded->suffix, suffix_bits > 0 ? 0xA5 : 0);
      EXPECT_EQ(decoded->suffix_bits, suffix_bits);
    }
  }
}

TEST(KeeloqProtocolTest, ASuffixEndsAtAGap) {
  // What follows the gap is the next frame.
  auto decoded = decode_with_suffix_space(2, 4 * BIT_US);
  ASSERT_TRUE(decoded.has_value());
  if (decoded.has_value()) {
    EXPECT_EQ(decoded->suffix, 0x5);
    EXPECT_EQ(decoded->suffix_bits, 3);
  }
}

TEST(KeeloqProtocolTest, DecodesAFrameWithLeadingCarrier) {
  auto decoded = decode(jarolift_emulator_frame());
  ASSERT_TRUE(decoded.has_value());
  if (decoded.has_value()) {
    EXPECT_EQ(decoded->encrypted, 0xF94A09F2u);
    EXPECT_EQ(decoded->address, 0x1234500u);
    EXPECT_EQ(decoded->command, 0x8);
    // The 8 bits after the 64-bit word: 0x0F arrives as vlow 1, repeat 1 and suffix 0x03.
    EXPECT_TRUE(decoded->vlow);
    EXPECT_TRUE(decoded->repeat);
    EXPECT_EQ(decoded->suffix, 0x03);
    EXPECT_EQ(decoded->suffix_bits, 6);
  }
}

TEST(KeeloqProtocolTest, LeadingCarrierNeedsTheFullPreamble) {
  // Without one of the 12 preamble marks after the burst there is no frame.
  RawTimings timings = jarolift_emulator_frame();
  timings.erase(timings.begin() + 2, timings.begin() + 4);
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(KeeloqProtocolTest, HcsWordStillEndsWithRepeatOne) {
  // 32 code bits, 28 address bits, 4 command bits, vlow, repeat.
  std::vector<bool> bits = sent_bits(encode(frame(0, 0)));
  ASSERT_EQ(bits.size(), 66u);
  EXPECT_FALSE(bits[64]);
  EXPECT_TRUE(bits[65]);
}

TEST(KeeloqProtocolTest, TdefGroupFrameSendsTheGroupAfterThe64BitWord) {
  std::vector<bool> bits = sent_bits(encode(tdef_frame(0x0F)));
  ASSERT_EQ(bits.size(), 72u);
  EXPECT_EQ(field(bits, 0, 32), 0xF94A09F2u);
  EXPECT_EQ(field(bits, 32, 28), 0x1234500u);
  EXPECT_EQ(field(bits, 60, 4), 0x8u);
  EXPECT_EQ(field(bits, 64, 8), 0x0Fu);
}

TEST(KeeloqProtocolTest, TdefChannelOneSendsNoChannelTenBit) {
  // Channels 1 to 8 have no bit in the group. Bit 65 is the HCS301 repeat bit, which is always 1
  // without a group; a Jarolift receiver reads it as channel 10.
  std::vector<bool> bits = sent_bits(encode(tdef_frame(0x00)));
  ASSERT_EQ(bits.size(), 72u);
  EXPECT_EQ(field(bits, 64, 8), 0x00u);
}

TEST(KeeloqProtocolTest, TdefGroupRoundTrips) {
  // 0x0F is channels 9 to 12, 0x80 is channel 16, the last bit of the frame.
  for (uint8_t group : {0x0F, 0x80}) {
    auto decoded = decode(as_received(encode(tdef_frame(group))));
    ASSERT_TRUE(decoded.has_value()) << int(group);
    if (decoded.has_value()) {
      EXPECT_EQ(decoded->address, 0x1234500u);
      EXPECT_EQ(decoded->command, 0x8);
      EXPECT_EQ(decoded->encrypted, 0xF94A09F2u);
      EXPECT_EQ(decoded->suffix_bits, 6);
      EXPECT_EQ(decoded->group(), group);
    }
  }
}

TEST(KeeloqProtocolTest, AReceivedRepeatBitOfZeroIsSentAgainAsZero) {
  KeeloqData received = frame(0, 0);
  received.repeat = false;
  std::vector<bool> bits = sent_bits(encode(received));
  ASSERT_EQ(bits.size(), 66u);
  EXPECT_FALSE(bits[65]);

  auto decoded = decode(as_received(encode(received)));
  ASSERT_TRUE(decoded.has_value());
  if (decoded.has_value()) {
    EXPECT_FALSE(decoded->repeat);
  }
}

}  // namespace esphome::remote_base::testing
