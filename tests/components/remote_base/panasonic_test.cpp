#include <gtest/gtest.h>
#include "esphome/components/remote_base/panasonic_protocol.h"

namespace esphome::remote_base::testing {

namespace {

PanasonicData frame(uint8_t nbits) {
  PanasonicData data{0x4004, 0x0100BCBD};
  data.nbits = nbits;
  if (nbits == 56) {
    data.address_2 = 0x12;
  }
  return data;
}

RawTimings encode(const PanasonicData &data) {
  RemoteTransmitData tx;
  PanasonicProtocol().encode(&tx, data);
  return tx.get_data();
}

optional<PanasonicData> decode(const RawTimings &timings) {
  return PanasonicProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

}  // namespace

TEST(PanasonicProtocolTest, DefaultsSendA48BitFrameAt35kHz) {
  RemoteTransmitData tx;
  PanasonicProtocol().encode(&tx, PanasonicData{0x4004, 0x0100BCBD});
  EXPECT_EQ(tx.get_data().size(), 2u + 2u * 48u + 1u);
  EXPECT_EQ(tx.get_carrier_frequency(), 35000u);
}

TEST(PanasonicProtocolTest, ForwardsTheConfiguredCarrier) {
  PanasonicData data = frame(56);
  data.carrier_frequency = 36000;
  RemoteTransmitData tx;
  PanasonicProtocol().encode(&tx, data);
  EXPECT_EQ(tx.get_carrier_frequency(), 36000u);
}

TEST(PanasonicProtocolTest, RoundTrips48BitFrame) {
  auto decoded = decode(encode(frame(48)));
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(*decoded, frame(48));
  EXPECT_EQ(decoded->nbits, 48);
  EXPECT_EQ(decoded->address_2, 0);
}

TEST(PanasonicProtocolTest, RoundTrips56BitFrame) {
  RawTimings timings = encode(frame(56));
  EXPECT_EQ(timings.size(), 2u + 2u * 56u + 1u);
  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(*decoded, frame(56));
  EXPECT_EQ(decoded->nbits, 56);
  EXPECT_EQ(decoded->address_2, 0x12);
  EXPECT_EQ(decoded->address, 0x4004);
  EXPECT_EQ(decoded->command, 0x0100BCBDu);
}

TEST(PanasonicProtocolTest, Decodes48BitFrameWithTrailingItems) {
  RawTimings timings = encode(frame(48));
  // A repeat frame arriving inside the idle window.
  timings.insert(timings.end(), {-20000, 3502, -1750, 502, -400});
  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_EQ(*decoded, frame(48));
}

TEST(PanasonicProtocolTest, RejectsTruncatedFrame) {
  RawTimings timings = encode(frame(48));
  timings.resize(timings.size() - 4);
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(PanasonicProtocolTest, InvalidNbitsSendsNothing) {
  PanasonicData data = frame(48);
  data.nbits = 40;
  EXPECT_TRUE(encode(data).empty());
}

TEST(PanasonicProtocolTest, EqualityIgnoresCarrier) {
  PanasonicData a = frame(56);
  PanasonicData b = frame(56);
  b.carrier_frequency = 36000;
  EXPECT_TRUE(a == b);
  b.address_2 = 0x13;
  EXPECT_FALSE(a == b);
  EXPECT_FALSE(frame(48) == frame(56));
}

}  // namespace esphome::remote_base::testing
