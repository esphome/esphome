#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "../common.h"

namespace esphome::aranet::testing {
namespace {

TEST(Aranet, DecodesAranet4) {
  Harness h;
  auto device = make_device(ARANET4_PAYLOAD);
  ASSERT_EQ(device.address_uint64(), ADDRESS);

  EXPECT_TRUE(h.aranet.parse_device(device));
  EXPECT_FLOAT_EQ(h.co2.state, 600.0f);
  EXPECT_FLOAT_EQ(h.temperature.state, 21.0f);
  EXPECT_FLOAT_EQ(h.humidity.state, 45.0f);
  EXPECT_FLOAT_EQ(h.pressure.state, 1013.2f);
  EXPECT_FLOAT_EQ(h.battery.state, 87.0f);
  EXPECT_FLOAT_EQ(h.interval.state, 300.0f);
  EXPECT_FLOAT_EQ(h.age.state, 12.0f);
  EXPECT_FLOAT_EQ(h.signal_strength.state, -61.0f);
  EXPECT_FALSE(h.radon.has_state());
  EXPECT_FALSE(h.radiation_rate.has_state());
  EXPECT_FALSE(h.radiation_total.has_state());
  EXPECT_FALSE(h.radiation_duration.has_state());
}

TEST(Aranet, UpdatesDiagnosticsForRepeatedMeasurement) {
  Harness h;
  int co2_updates = 0;
  int age_updates = 0;
  int signal_strength_updates = 0;
  h.co2.add_on_state_callback([&co2_updates](float) { co2_updates++; });
  h.age.add_on_state_callback([&age_updates](float) { age_updates++; });
  h.signal_strength.add_on_state_callback([&signal_strength_updates](float) { signal_strength_updates++; });

  EXPECT_TRUE(h.aranet.parse_device(make_device(ARANET4_PAYLOAD, -61)));
  auto payload = ARANET4_PAYLOAD;
  payload[19] = 18;
  EXPECT_TRUE(h.aranet.parse_device(make_device(payload, -54)));

  EXPECT_EQ(co2_updates, 1);
  EXPECT_EQ(age_updates, 2);
  EXPECT_EQ(signal_strength_updates, 2);
  EXPECT_FLOAT_EQ(h.age.state, 18.0f);
  EXPECT_FLOAT_EQ(h.signal_strength.state, -54.0f);
}

TEST(Aranet, RejectsDisabledIntegrationAndMalformedPayload) {
  Harness h;
  auto payload = ARANET4_PAYLOAD;

  payload[0] = 0;
  EXPECT_FALSE(h.aranet.parse_device(make_device(payload)));

  payload.pop_back();
  EXPECT_FALSE(h.aranet.parse_device(make_device(payload)));
  EXPECT_FALSE(h.co2.has_state());
  EXPECT_FALSE(h.signal_strength.has_state());
}

TEST(Aranet, RejectsMismatchedAddressAndUnsupportedDeviceType) {
  Harness h;
  constexpr uint8_t other_mac_address[6] = {0x66, 0x44, 0x33, 0x22, 0x11, 0x00};
  EXPECT_FALSE(h.aranet.parse_device(make_device(ARANET4_PAYLOAD, -61, other_mac_address)));

  auto payload = ARANET4_PAYLOAD;
  payload.insert(payload.begin(), 0x09);
  payload.push_back(0x00);
  EXPECT_FALSE(h.aranet.parse_device(make_device(payload)));

  // Type 0 is Aranet4, which never sends the 24 byte layout; the second warning stays quiet
  payload[0] = 0x00;
  EXPECT_FALSE(h.aranet.parse_device(make_device(payload)));
  EXPECT_FALSE(h.signal_strength.has_state());
}

TEST(Aranet, DecodesAranet2) {
  Harness h;
  int temperature_updates = 0;
  h.temperature.add_on_state_callback([&temperature_updates](float) { temperature_updates++; });

  EXPECT_TRUE(h.aranet.parse_device(make_device(ARANET2_PAYLOAD)));
  EXPECT_NEAR(h.temperature.state, 20.45f, 0.001f);
  EXPECT_FLOAT_EQ(h.humidity.state, 52.2f);
  EXPECT_FLOAT_EQ(h.battery.state, 59.0f);
  EXPECT_FLOAT_EQ(h.interval.state, 120.0f);
  EXPECT_FLOAT_EQ(h.age.state, 82.0f);
  EXPECT_FALSE(h.co2.has_state());
  EXPECT_FALSE(h.pressure.has_state());

  auto repeated_payload = ARANET2_PAYLOAD;
  repeated_payload[10] = 0x00;
  repeated_payload[11] = 0x00;
  EXPECT_TRUE(h.aranet.parse_device(make_device(repeated_payload)));
  EXPECT_EQ(temperature_updates, 1);
}

TEST(Aranet, DecodesAranetRadiation) {
  Harness h;
  EXPECT_TRUE(h.aranet.parse_device(make_device(RADIATION_PAYLOAD)));
  EXPECT_FLOAT_EQ(h.radiation_rate.state, 0.13f);
  EXPECT_FLOAT_EQ(h.radiation_total.state, 13264.0f);
  EXPECT_FLOAT_EQ(h.radiation_duration.state, 417900.0f);
  EXPECT_FLOAT_EQ(h.battery.state, 99.0f);
  EXPECT_FLOAT_EQ(h.interval.state, 300.0f);
  EXPECT_FLOAT_EQ(h.age.state, 88.0f);
  EXPECT_FALSE(h.co2.has_state());
  EXPECT_FALSE(h.temperature.has_state());
  EXPECT_FALSE(h.humidity.has_state());
  EXPECT_FALSE(h.pressure.has_state());
  EXPECT_FALSE(h.radon.has_state());
}

TEST(Aranet, DecodesAranetRadon) {
  Harness h;
  EXPECT_TRUE(h.aranet.parse_device(make_device(RADON_PAYLOAD)));
  EXPECT_FLOAT_EQ(h.radon.state, 7.0f);
  EXPECT_FLOAT_EQ(h.temperature.state, 25.5f);
  EXPECT_FLOAT_EQ(h.humidity.state, 46.2f);
  EXPECT_FLOAT_EQ(h.pressure.state, 1018.5f);
  EXPECT_FALSE(h.co2.has_state());
  EXPECT_FALSE(h.radiation_rate.has_state());
  EXPECT_FALSE(h.radiation_total.has_state());
  EXPECT_FALSE(h.radiation_duration.has_state());
}

TEST(Aranet, RadonOneSkipsZeroTemperatureAndHumidity) {
  Harness h;
  auto payload = RADON_PAYLOAD;
  payload[10] = payload[11] = 0x00;
  payload[14] = payload[15] = 0x00;
  EXPECT_TRUE(h.aranet.parse_device(make_device(payload)));
  EXPECT_FLOAT_EQ(h.radon.state, 7.0f);
  EXPECT_FLOAT_EQ(h.pressure.state, 1018.5f);
  EXPECT_FALSE(h.temperature.has_state());
  EXPECT_FALSE(h.humidity.has_state());
}

}  // namespace
}  // namespace esphome::aranet::testing
