#include <array>
#include <gtest/gtest.h>

#include "esphome/components/remote_base/cardin_s438_protocol.h"

namespace esphome::remote_base::testing {
namespace {

constexpr int32_t SYNC_MARK_US = 6400;
constexpr int32_t SHORT_US = 750;
constexpr int32_t LONG_US = 1460;
constexpr int32_t FRAME_GAP_US = 63650;

constexpr size_t FRAME_TIMINGS_SIZE = 1 + 5 * 8 * 2 + 1;

// Captured frame: FA C3 EC A9 02, seed 0x65.
constexpr std::array<uint8_t, 5> CAPTURED_FRAME = {0xFA, 0xC3, 0xEC, 0xA9, 0x02};

// Reference frame for code 0x11FF, channel A, seed 0x65.
constexpr std::array<uint8_t, 5> REFERENCE_FRAME = {0xC9, 0x05, 0xEC, 0xBA, 0x01};

RawTimings encode(const CardinS438Data &data) {
  RemoteTransmitData tx;
  CardinS438Protocol().encode(&tx, data);
  return tx.get_data();
}

optional<CardinS438Data> decode(const RawTimings &timings) {
  return CardinS438Protocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

RawTimings frame_timings(const std::array<uint8_t, 5> &wire) {
  RawTimings timings{SYNC_MARK_US};

  for (uint8_t byte : wire) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if ((byte >> bit) & 1U) {
        timings.insert(timings.end(), {-SHORT_US, LONG_US});
      } else {
        timings.insert(timings.end(), {-LONG_US, SHORT_US});
      }
    }
  }

  timings.push_back(-FRAME_GAP_US);
  return timings;
}

}  // namespace

TEST(CardinS438ProtocolTest, DecodesCapturedFrame) {
  auto decoded = decode(frame_timings(CAPTURED_FRAME));

  ASSERT_TRUE(decoded.has_value());
  ASSERT_EQ(decoded->seed, 0x65);
  EXPECT_EQ(decoded->code, 0x2E12);
  EXPECT_EQ(decoded->channel, 0x02);
}

TEST(CardinS438ProtocolTest, DecodesReferenceFrame) {
  auto decoded = decode(frame_timings(REFERENCE_FRAME));

  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->code, 0x11FF);
  EXPECT_EQ(decoded->channel, 0x02);
  EXPECT_EQ(decoded->seed, 0x65);
}

TEST(CardinS438ProtocolTest, RoundTrips) {
  for (uint16_t code : {0x0000, 0x0001, 0x11FF, 0x2AAA, 0x3FFF}) {
    for (uint8_t channel = 0; channel < 4; ++channel) {
      CardinS438Data sent{};
      sent.code = code;
      sent.channel = channel;
      sent.repeats = 1;

      const RawTimings timings = encode(sent);
      ASSERT_EQ(timings.size(), FRAME_TIMINGS_SIZE);

      auto decoded = decode(timings);
      ASSERT_TRUE(decoded.has_value());
      EXPECT_TRUE(*decoded == sent);
    }
  }
}

TEST(CardinS438ProtocolTest, RejectsBadChecksum) {
  auto wire = CAPTURED_FRAME;
  wire[3] ^= 1;

  EXPECT_FALSE(decode(frame_timings(wire)).has_value());
}

TEST(CardinS438ProtocolTest, SkipsLeadingNoise) {
  RawTimings timings{300, -1200, 150, -90};
  const RawTimings frame = frame_timings(CAPTURED_FRAME);
  timings.insert(timings.end(), frame.begin(), frame.end());

  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->code, 0x2E12);
}

TEST(CardinS438ProtocolTest, FindsValidFrameAfterBadChecksum) {
  auto bad_wire = CAPTURED_FRAME;
  bad_wire[3] ^= 1;

  RawTimings timings = frame_timings(bad_wire);
  const RawTimings good = frame_timings(CAPTURED_FRAME);
  timings.insert(timings.end(), good.begin(), good.end());

  EXPECT_TRUE(decode(timings).has_value());
}

TEST(CardinS438ProtocolTest, RejectsBadStart) {
  RawTimings timings = frame_timings(CAPTURED_FRAME);
  timings[0] = 2000;

  EXPECT_FALSE(decode(timings).has_value());
}

TEST(CardinS438ProtocolTest, RejectsTruncatedFrame) {
  RawTimings timings = frame_timings(CAPTURED_FRAME);
  timings.resize(timings.size() - 6);

  EXPECT_FALSE(decode(timings).has_value());
}

TEST(CardinS438ProtocolTest, RejectsBadBitTiming) {
  RawTimings timings = frame_timings(CAPTURED_FRAME);
  timings[1] = -2000;

  EXPECT_FALSE(decode(timings).has_value());
}

TEST(CardinS438ProtocolTest, DecodesSeedCarryBoundaries) {
  struct SeedVector {
    uint8_t seed;
    std::array<uint8_t, 5> wire;
  };

  // All vectors encode code=0x11FF, channel A.
  // 0x78: marker sum is 0xFF; second carry occurs.
  // 0x79: marker and second carry both occur.
  // 0xFF: marker carry wraps the key to zero.
  constexpr SeedVector frames[] = {
      {0x78, {0xDC, 0x18, 0xFF, 0xF3, 0x01}},
      {0x79, {0xDE, 0x1A, 0x00, 0xF8, 0x00}},
      {0xFF, {0x63, 0xA0, 0x86, 0x89, 0x01}},
  };

  for (const auto &frame : frames) {
    const auto decoded = decode(frame_timings(frame.wire));

    ASSERT_TRUE(decoded.has_value()) << "seed=0x" << std::hex << unsigned(frame.seed);
    EXPECT_EQ(decoded->code, 0x11FF);
    EXPECT_EQ(decoded->channel, 0x02);
    EXPECT_EQ(decoded->seed, frame.seed);
  }
}

TEST(CardinS438ProtocolTest, EncodesRequestedFrameCount) {
  for (uint8_t repeats : {1, 3, 5}) {
    CardinS438Data sent{};
    sent.code = 0x11FF;
    sent.channel = 0x02;
    sent.repeats = repeats;

    const RawTimings timings = encode(sent);

    ASSERT_EQ(timings.size(), FRAME_TIMINGS_SIZE * repeats);

    // Check each independently seeded frame, without assuming seeds differ.
    for (size_t i = 0; i < repeats; ++i) {
      const auto first = timings.begin() + i * FRAME_TIMINGS_SIZE;
      const RawTimings frame(first, first + FRAME_TIMINGS_SIZE);
      const auto decoded = decode(frame);

      ASSERT_TRUE(decoded.has_value()) << "frame=" << i;
      EXPECT_EQ(decoded->code, sent.code);
      EXPECT_EQ(decoded->channel, sent.channel);
    }
  }
}

}  // namespace esphome::remote_base::testing
