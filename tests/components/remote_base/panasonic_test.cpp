#include <gtest/gtest.h>
#include "esphome/components/remote_base/panasonic_protocol.h"

namespace esphome::remote_base::testing {

namespace {

const PanasonicData FRAME_48{0x4004, 0x0100BCBD};
const PanasonicData FRAME_56{0x4004, 0x0100BCBD, 56, 0x12};

RawTimings encode(const PanasonicData &data) {
  RemoteTransmitData tx;
  PanasonicProtocol().encode(&tx, data);
  return tx.get_data();
}

optional<PanasonicData> decode(const RawTimings &timings) {
  return PanasonicProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

}  // namespace

TEST(PanasonicProtocolTest, ForwardsTheConfiguredCarrier) {
  PanasonicData data = FRAME_56;
  data.carrier_frequency = 36000;
  RemoteTransmitData tx;
  PanasonicProtocol().encode(&tx, data);
  EXPECT_EQ(tx.get_carrier_frequency(), 36000u);
}

TEST(PanasonicProtocolTest, RoundTrips48BitFrame) {
  RemoteTransmitData tx;
  PanasonicProtocol().encode(&tx, FRAME_48);
  EXPECT_EQ(tx.get_data().size(), 2u + 2u * 48u + 1u);
  EXPECT_EQ(tx.get_carrier_frequency(), 35000u);
  auto decoded = decode(tx.get_data());
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(*decoded, FRAME_48);
}

TEST(PanasonicProtocolTest, RoundTrips56BitFrame) {
  RawTimings timings = encode(FRAME_56);
  EXPECT_EQ(timings.size(), 2u + 2u * 56u + 1u);
  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(*decoded, FRAME_56);
}

TEST(PanasonicProtocolTest, Decodes56BitFrameWithTrailingItems) {
  RawTimings timings = encode(FRAME_56);
  timings.insert(timings.end(), {-20000, 3502, -1750, 502, -400});
  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(*decoded, FRAME_56);
}

TEST(PanasonicProtocolTest, Decodes48BitFrameWithTrailingItems) {
  RawTimings timings = encode(FRAME_48);
  // A repeat frame arriving inside the idle window.
  timings.insert(timings.end(), {-20000, 3502, -1750, 502, -400});
  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(*decoded, FRAME_48);
}

TEST(PanasonicProtocolTest, RejectsTruncatedFrame) {
  RawTimings timings = encode(FRAME_48);
  timings.resize(timings.size() - 4);
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(PanasonicProtocolTest, InvalidNbitsSendsNothing) {
  PanasonicData data = FRAME_48;
  data.nbits = 40;
  EXPECT_TRUE(encode(data).empty());
}

TEST(PanasonicProtocolTest, EqualityIgnoresCarrier) {
  PanasonicData other_carrier = FRAME_56;
  other_carrier.carrier_frequency = 36000;
  EXPECT_EQ(FRAME_56, other_carrier);
  PanasonicData other_address_2 = FRAME_56;
  other_address_2.address_2 = 0x13;
  EXPECT_NE(FRAME_56, other_address_2);
  EXPECT_NE(FRAME_48, FRAME_56);
}

}  // namespace esphome::remote_base::testing
