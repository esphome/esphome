#include <gtest/gtest.h>
#include "esphome/components/remote_base/magiquest_protocol.h"

namespace esphome::remote_base::testing {

namespace {

constexpr int32_t UNIT = 288;
constexpr uint32_t WAND_ID = 0x2A5B3C1D;
constexpr uint16_t MAGNITUDE = 258;
constexpr MagiQuestData DECODED{.magnitude = MAGNITUDE, .wand_id = WAND_ID};

void add_bit(RawTimings &timings, bool one, bool last = false) {
  timings.push_back(one ? 2 * UNIT : UNIT);
  if (!last) {
    timings.push_back(-(one ? 2 * UNIT : 3 * UNIT));
  }
}

// Overwrite bit `bit` of a built frame with a one
void set_one(RawTimings &timings, size_t bit) {
  timings[2 * bit] = 2 * UNIT;
  timings[2 * bit + 1] = -2 * UNIT;
}

// Build a frame the way a wand sends it: 8 zero header bits, a 31 bit wand id, a 9 bit magnitude and a checksum
// that makes the six payload bytes sum to zero. The last bit has no trailing space.
RawTimings build_frame(uint32_t wand_id, uint16_t magnitude, uint8_t checksum_offset = 0) {
  const uint32_t id_bytes = wand_id << 1;
  const uint32_t magnitude_bits = uint32_t(magnitude) << 8;
  uint8_t sum = 0;
  for (uint8_t shift = 0; shift < 32; shift += 8) {
    sum += (id_bytes >> shift) + (magnitude_bits >> shift);
  }
  const uint32_t magnitude_and_checksum = magnitude_bits | uint8_t(-sum + checksum_offset);

  RawTimings timings;
  for (int i = 0; i < 8; i++) {
    add_bit(timings, false);
  }
  for (uint32_t mask = 1 << 30; mask; mask >>= 1) {
    add_bit(timings, wand_id & mask);
  }
  for (uint32_t mask = 1 << 16; mask; mask >>= 1) {
    add_bit(timings, magnitude_and_checksum & mask, mask == 1);
  }
  return timings;
}

// The receiver's tolerance is passed for completeness; decode() replaces it with the protocol's fixed window
optional<MagiQuestData> decode(const RawTimings &timings) {
  return MagiQuestProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

void expect_decodes_to(const RawTimings &timings, uint32_t wand_id, uint16_t magnitude) {
  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  // clang-tidy's unchecked-optional-access models neither gtest's ASSERT_TRUE nor value() as a check
  if (decoded.has_value()) {
    EXPECT_EQ(decoded->wand_id, wand_id);
    EXPECT_EQ(decoded->magnitude, magnitude);
  }
}

}  // namespace

TEST(MagiQuestProtocolTest, DecodesAValidFrame) {
  expect_decodes_to(build_frame(WAND_ID, MAGNITUDE), WAND_ID, MAGNITUDE);
}

// Magnitude 1 with wand id 0 needs a checksum of 0xFF, so the last bit is a one; magnitude 2 needs 0xFE.
TEST(MagiQuestProtocolTest, DecodesBothValuesOfTheLastBit) {
  expect_decodes_to(build_frame(0, 1), 0, 1);
  expect_decodes_to(build_frame(0, 2), 0, 2);
}

TEST(MagiQuestProtocolTest, RejectsABadChecksum) {
  EXPECT_FALSE(decode(build_frame(WAND_ID, MAGNITUDE, 1)).has_value());
}

TEST(MagiQuestProtocolTest, RejectsAFlippedDataBit) {
  auto timings = build_frame(WAND_ID, MAGNITUDE);
  set_one(timings, 8);  // first wand id bit, a zero in WAND_ID
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(MagiQuestProtocolTest, RejectsANonZeroHeader) {
  auto timings = build_frame(WAND_ID, MAGNITUDE);
  set_one(timings, 0);
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(MagiQuestProtocolTest, RejectsATruncatedFrame) {
  auto timings = build_frame(WAND_ID, MAGNITUDE);
  timings.resize(timings.size() - 10);
  EXPECT_FALSE(decode(timings).has_value());
}

// Receivers stretch marks and shrink spaces; 120 us is more than 25% of a unit but within the fixed window.
TEST(MagiQuestProtocolTest, DecodesWithReceiverJitterUnderTheDefaultTolerance) {
  auto timings = build_frame(WAND_ID, MAGNITUDE);
  for (auto &timing : timings) {
    timing += 120;
  }
  expect_decodes_to(timings, WAND_ID, MAGNITUDE);
}

TEST(MagiQuestProtocolTest, MatchesAConfiguredWand) {
  EXPECT_TRUE(DECODED == (MagiQuestData{.magnitude = 0, .wand_id = WAND_ID}));
  EXPECT_FALSE(DECODED == (MagiQuestData{.magnitude = 0, .wand_id = WAND_ID + 1}));
}

TEST(MagiQuestProtocolTest, MatchesAnyWandWhenNoIdIsConfigured) {
  EXPECT_TRUE(DECODED == (MagiQuestData{.magnitude = 0, .wand_id = 0}));
}

// Configs written for the old decoder hold the id without its 5 low bits.
TEST(MagiQuestProtocolTest, MatchesAnIdFromTheOldDecoder) {
  EXPECT_TRUE(DECODED == (MagiQuestData{.magnitude = 0, .wand_id = WAND_ID >> 5}));
}

TEST(MagiQuestProtocolTest, IgnoresMagnitudeWhenMatching) {
  EXPECT_TRUE(DECODED == (MagiQuestData{.magnitude = 0xFFFF, .wand_id = WAND_ID}));
}

}  // namespace esphome::remote_base::testing
