#include <gtest/gtest.h>
#include "esphome/components/remote_base/denon_protocol.h"

namespace esphome::remote_base::testing {

namespace {

// One frame as Denon's published Pronto codes send it: 15 bits in transmission order, then the stop mark
RawTimings reference_frame(uint16_t bits) {
  RawTimings out;
  for (int bit = 14; bit >= 0; bit--) {
    out.push_back(263);
    out.push_back((bits >> bit) & 1 ? -1841 : -789);
  }
  out.push_back(263);
  return out;
}

// The frame pair: the second frame inverts the last ten bits
RawTimings reference_pair(uint16_t bits) {
  RawTimings out = reference_frame(bits);
  out.push_back(-43572);
  RawTimings second = reference_frame(bits ^ 0x3FF);
  out.insert(out.end(), second.begin(), second.end());
  return out;
}

optional<DenonData> decode(const RawTimings &timings) {
  return DenonProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

}  // namespace

// Device code and function number from Denon's IR code tables, with the frame from Denon's Pronto code
TEST(DenonProtocolTest, EncodesDenonsPublishedFrames) {
  struct Case {
    uint8_t address;
    uint8_t command;
    uint16_t frame;
  };
  for (const Case &c : {Case{2, 121, 0x2278}, Case{4, 129, 0x1204}, Case{2, 241, 0x223C}}) {
    RemoteTransmitData tx;
    DenonProtocol().encode(&tx, DenonData{.address = c.address, .command = c.command});
    EXPECT_EQ(tx.get_data(), reference_pair(c.frame)) << int(c.address) << "/" << int(c.command);
  }
}

TEST(DenonProtocolTest, DecodesTheFirstFrame) {
  auto decoded = decode(reference_frame(0x1204));
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(decoded->address, 4);
  EXPECT_EQ(decoded->command, 129);
}

TEST(DenonProtocolTest, DecodesThePairOnce) {
  auto decoded = decode(reference_pair(0x2278));
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(decoded->address, 2);
  EXPECT_EQ(decoded->command, 121);
}

TEST(DenonProtocolTest, IgnoresTheInvertedFrame) { EXPECT_FALSE(decode(reference_frame(0x2278 ^ 0x3FF)).has_value()); }

TEST(DenonProtocolTest, RoundTrips) {
  for (const uint8_t address : {0, 1, 2, 17, 31}) {
    for (const uint8_t command : {0, 1, 0x80, 0xA5, 0xFF}) {
      RemoteTransmitData tx;
      DenonProtocol().encode(&tx, DenonData{.address = address, .command = command});
      auto decoded = decode(tx.get_data());
      ASSERT_TRUE(decoded.has_value()) << int(address) << "/" << int(command);
      if (!decoded.has_value()) {
        return;
      }
      EXPECT_EQ(decoded->address, address);
      EXPECT_EQ(decoded->command, command);
    }
  }
}

TEST(DenonProtocolTest, RejectsABadSpace) {
  RawTimings timings = reference_frame(0x2278);
  timings[5] = -1300;
  EXPECT_FALSE(decode(timings).has_value());
}

}  // namespace esphome::remote_base::testing
