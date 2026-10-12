#include <gtest/gtest.h>

#include <cmath>
#include "esphome/components/remote_base/nexus_protocol.h"

namespace esphome::remote_base::testing {

namespace {

RawTimings encode(const NexusData &data) {
  RemoteTransmitData tx;
  NexusProtocol().encode(&tx, data);
  return tx.get_data();
}

optional<NexusData> decode(const RawTimings &timings) {
  return NexusProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

void expect_round_trip(float temperature, bool drop_sync) {
  const NexusData sent{.temperature = temperature,
                       .address = 0x85,
                       .battery_level = true,
                       .force_update = false,
                       .channel = 2,
                       .humidity = 69};
  RawTimings timings = encode(sent);
  ASSERT_EQ(timings.size(), 2u + 36u * 2u);
  if (drop_sync) {
    timings.resize(timings.size() - 2);
  } else {
    // The receiver kept the previous frame's sync in front of this one
    timings.insert(timings.begin(), timings.end() - 2, timings.end());
  }
  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value()) << temperature;
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_FLOAT_EQ(decoded->temperature, temperature);
  EXPECT_EQ(decoded->address, 0x85);
  EXPECT_TRUE(decoded->battery_level);
  EXPECT_FALSE(decoded->force_update);
  EXPECT_EQ(decoded->channel, 2);
  EXPECT_EQ(decoded->humidity, 69);
}

}  // namespace

TEST(NexusProtocolTest, RoundTripsWithLeadingSync) {
  expect_round_trip(13.8f, false);
  expect_round_trip(-12.3f, false);
  expect_round_trip(-204.8f, false);
  expect_round_trip(204.7f, false);
}

TEST(NexusProtocolTest, RoundTripsWithoutTrailingSync) {
  expect_round_trip(0.0f, true);
  expect_round_trip(-0.1f, true);
}

TEST(NexusProtocolTest, ClampsOutOfRangeTemperature) {
  auto decoded = decode(encode(NexusData{.temperature = 300.0f, .channel = 1}));
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_FLOAT_EQ(decoded->temperature, 204.7f);
  decoded = decode(encode(NexusData{.temperature = NAN, .channel = 1}));
  ASSERT_TRUE(decoded.has_value());
  if (!decoded.has_value()) {
    return;
  }
  EXPECT_FLOAT_EQ(decoded->temperature, 0.0f);
}

TEST(NexusProtocolTest, RejectsWrongConstantBits) {
  RawTimings timings = encode(NexusData{.temperature = 20.0f, .channel = 1});
  // Constant bits are data bits 24-27; each bit is a mark and a space, the sync comes last.
  timings[24 * 2 + 1] = -980;
  EXPECT_FALSE(decode(timings).has_value());
}

}  // namespace esphome::remote_base::testing
