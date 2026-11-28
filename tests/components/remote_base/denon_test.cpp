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
TEST(DenonProtocolTest, MatchesDenonsPublishedFrames) {
  struct Case {
    DenonData data;
    uint16_t frame;
  };
  for (const Case &c : {Case{{2, 121}, 0x2278}, Case{{4, 129}, 0x1204}, Case{{2, 241}, 0x223C}}) {
    SCOPED_TRACE(::testing::Message() << int(c.data.address) << "/" << int(c.data.command));
    RemoteTransmitData tx;
    DenonProtocol().encode(&tx, c.data);
    EXPECT_EQ(tx.get_data(), reference_pair(c.frame));
    // The first frame alone and the whole pair both decode once
    for (const RawTimings &timings : {reference_frame(c.frame), reference_pair(c.frame)}) {
      auto decoded = decode(timings);
      ASSERT_TRUE(decoded.has_value());
      if (!decoded.has_value()) {
        return;
      }
      EXPECT_EQ(*decoded, c.data);
    }
  }
}

TEST(DenonProtocolTest, IgnoresTheInvertedFrame) { EXPECT_FALSE(decode(reference_frame(0x2278 ^ 0x3FF)).has_value()); }

TEST(DenonProtocolTest, RoundTrips) {
  for (const uint8_t address : {0, 1, 2, 17, 31}) {
    for (const uint8_t command : {0, 1, 0x80, 0xA5, 0xFF}) {
      const DenonData data{.address = address, .command = command};
      RemoteTransmitData tx;
      DenonProtocol().encode(&tx, data);
      auto decoded = decode(tx.get_data());
      ASSERT_TRUE(decoded.has_value()) << int(address) << "/" << int(command);
      if (!decoded.has_value()) {
        return;
      }
      EXPECT_EQ(*decoded, data);
    }
  }
}

// A lambda can pass an address above 31; its extra bits must not reach the command
TEST(DenonProtocolTest, AddressAbove31KeepsTheCommand) {
  RemoteTransmitData wide, masked;
  DenonProtocol().encode(&wide, DenonData{.address = 34, .command = 120});
  DenonProtocol().encode(&masked, DenonData{.address = 2, .command = 120});
  EXPECT_EQ(wide.get_data(), masked.get_data());
}

TEST(DenonProtocolTest, RejectsABadSpace) {
  RawTimings timings = reference_frame(0x2278);
  timings[5] = -1300;
  EXPECT_FALSE(decode(timings).has_value());
}

}  // namespace esphome::remote_base::testing
