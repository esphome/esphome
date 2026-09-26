#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "../common.h"
#include "esphome/components/aranet/aranet.h"

namespace esphome::aranet::testing {
namespace {

ble_device_base::ESPBTDevice make_device(const std::vector<uint8_t> &payload, int rssi = -61,
                                         const uint8_t *mac_address = MAC_ADDRESS) {
  std::vector<uint8_t> advertisement{static_cast<uint8_t>(payload.size() + 3), 0xFF, 0x02, 0x07};
  advertisement.insert(advertisement.end(), payload.begin(), payload.end());

  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac_address, rssi, 0, advertisement.data(), advertisement.size());
  return device;
}

TEST(Aranet, DecodesManufacturerData) {
  auto payload = make_payload();
  auto device = make_device({payload.begin(), payload.end()});
  Aranet aranet(device.address_uint64());
  sensor::Sensor co2;
  sensor::Sensor temperature;
  sensor::Sensor humidity;
  sensor::Sensor pressure;
  sensor::Sensor battery;
  sensor::Sensor interval;
  sensor::Sensor age;
  sensor::Sensor signal_strength;
  aranet.set_co2_sensor(&co2);
  aranet.set_temperature_sensor(&temperature);
  aranet.set_humidity_sensor(&humidity);
  aranet.set_pressure_sensor(&pressure);
  aranet.set_battery_level_sensor(&battery);
  aranet.set_measurement_interval_sensor(&interval);
  aranet.set_measurement_age_sensor(&age);
  aranet.set_signal_strength_sensor(&signal_strength);

  EXPECT_TRUE(aranet.parse_device(device));
  EXPECT_FLOAT_EQ(co2.state, 600.0f);
  EXPECT_FLOAT_EQ(temperature.state, 21.0f);
  EXPECT_FLOAT_EQ(humidity.state, 45.0f);
  EXPECT_FLOAT_EQ(pressure.state, 1013.2f);
  EXPECT_FLOAT_EQ(battery.state, 87.0f);
  EXPECT_FLOAT_EQ(interval.state, 300.0f);
  EXPECT_FLOAT_EQ(age.state, 12.0f);
  EXPECT_FLOAT_EQ(signal_strength.state, -61.0f);
}

TEST(Aranet, UpdatesDiagnosticsForRepeatedMeasurement) {
  auto payload = make_payload();
  auto first_device = make_device({payload.begin(), payload.end()}, -61);
  Aranet aranet(first_device.address_uint64());
  sensor::Sensor co2;
  sensor::Sensor age;
  sensor::Sensor signal_strength;
  int co2_updates = 0;
  int age_updates = 0;
  int signal_strength_updates = 0;
  co2.add_on_state_callback([&co2_updates](float) { co2_updates++; });
  age.add_on_state_callback([&age_updates](float) { age_updates++; });
  signal_strength.add_on_state_callback([&signal_strength_updates](float) { signal_strength_updates++; });
  aranet.set_co2_sensor(&co2);
  aranet.set_measurement_age_sensor(&age);
  aranet.set_signal_strength_sensor(&signal_strength);

  EXPECT_TRUE(aranet.parse_device(first_device));
  payload[19] = 18;
  auto second_device = make_device({payload.begin(), payload.end()}, -54);
  EXPECT_TRUE(aranet.parse_device(second_device));

  EXPECT_EQ(co2_updates, 1);
  EXPECT_EQ(age_updates, 2);
  EXPECT_EQ(signal_strength_updates, 2);
  EXPECT_FLOAT_EQ(age.state, 18.0f);
  EXPECT_FLOAT_EQ(signal_strength.state, -54.0f);
}

TEST(Aranet, RejectsDisabledIntegrationAndMalformedPayload) {
  const auto valid_payload = make_payload();
  std::vector<uint8_t> payload(valid_payload.begin(), valid_payload.end());
  auto valid_device = make_device({payload.begin(), payload.end()});
  Aranet aranet(valid_device.address_uint64());

  payload[0] = 0;
  EXPECT_FALSE(aranet.parse_device(make_device({payload.begin(), payload.end()})));

  payload.pop_back();
  EXPECT_FALSE(aranet.parse_device(make_device({payload.begin(), payload.end()})));
}

TEST(Aranet, RejectsMismatchedAddressAndUnsupportedDeviceType) {
  const auto valid_payload = make_payload();
  std::vector<uint8_t> payload(valid_payload.begin(), valid_payload.end());
  auto valid_device = make_device(payload);
  Aranet aranet(valid_device.address_uint64());

  constexpr uint8_t other_mac_address[6] = {0x66, 0x44, 0x33, 0x22, 0x11, 0x00};
  EXPECT_FALSE(aranet.parse_device(make_device(payload, -61, other_mac_address)));

  payload.insert(payload.begin(), 0x09);
  payload.push_back(0x00);
  EXPECT_FALSE(aranet.parse_device(make_device(payload)));
  EXPECT_FALSE(aranet.parse_device(make_device(payload)));
}

TEST(Aranet, DecodesAranet2) {
  const std::vector<uint8_t> payload{0x01, 0x21, 0x04, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x99, 0x01,
                                     0x00, 0x00, 0x0A, 0x02, 0x00, 0x3B, 0x09, 0x78, 0x00, 0x52, 0x00, 0x64};
  auto device = make_device(payload);
  Aranet aranet(device.address_uint64());
  sensor::Sensor temperature;
  sensor::Sensor humidity;
  sensor::Sensor battery;
  sensor::Sensor interval;
  sensor::Sensor age;
  int temperature_updates = 0;
  temperature.add_on_state_callback([&temperature_updates](float) { temperature_updates++; });
  aranet.set_temperature_sensor(&temperature);
  aranet.set_humidity_sensor(&humidity);
  aranet.set_battery_level_sensor(&battery);
  aranet.set_measurement_interval_sensor(&interval);
  aranet.set_measurement_age_sensor(&age);

  EXPECT_TRUE(aranet.parse_device(device));
  EXPECT_NEAR(temperature.state, 20.45f, 0.001f);
  EXPECT_FLOAT_EQ(humidity.state, 52.2f);
  EXPECT_FLOAT_EQ(battery.state, 59.0f);
  EXPECT_FLOAT_EQ(interval.state, 120.0f);
  EXPECT_FLOAT_EQ(age.state, 82.0f);

  auto repeated_payload = payload;
  repeated_payload[10] = 0x00;
  repeated_payload[11] = 0x00;
  EXPECT_TRUE(aranet.parse_device(make_device(repeated_payload)));
  EXPECT_EQ(temperature_updates, 1);
}

TEST(Aranet, DecodesAranetRadiation) {
  const std::vector<uint8_t> payload{0x02, 0x21, 0x26, 0x04, 0x01, 0x00, 0xD0, 0x33, 0x00, 0x00, 0x6C, 0x60,
                                     0x06, 0x00, 0x82, 0x00, 0x00, 0x63, 0x00, 0x2C, 0x01, 0x58, 0x00, 0x72};
  auto device = make_device(payload);
  Aranet aranet(device.address_uint64());
  sensor::Sensor rate;
  sensor::Sensor total;
  sensor::Sensor duration;
  aranet.set_radiation_rate_sensor(&rate);
  aranet.set_radiation_total_sensor(&total);
  aranet.set_radiation_duration_sensor(&duration);

  EXPECT_TRUE(aranet.parse_device(device));
  EXPECT_FLOAT_EQ(rate.state, 0.13f);
  EXPECT_FLOAT_EQ(total.state, 13264.0f);
  EXPECT_FLOAT_EQ(duration.state, 417900.0f);
}

TEST(Aranet, DecodesAranetRadon) {
  const std::vector<uint8_t> payload{0x03, 0x21, 0x04, 0x06, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0xFE, 0x01,
                                     0xC9, 0x27, 0xCE, 0x01, 0x00, 0x64, 0x01, 0x58, 0x02, 0xF6, 0x01, 0x08};
  auto device = make_device(payload);
  Aranet aranet(device.address_uint64());
  sensor::Sensor radon;
  sensor::Sensor temperature;
  sensor::Sensor humidity;
  sensor::Sensor pressure;
  aranet.set_radon_sensor(&radon);
  aranet.set_temperature_sensor(&temperature);
  aranet.set_humidity_sensor(&humidity);
  aranet.set_pressure_sensor(&pressure);

  EXPECT_TRUE(aranet.parse_device(device));
  EXPECT_FLOAT_EQ(radon.state, 7.0f);
  EXPECT_FLOAT_EQ(temperature.state, 25.5f);
  EXPECT_FLOAT_EQ(humidity.state, 46.2f);
  EXPECT_FLOAT_EQ(pressure.state, 1018.5f);
}

}  // namespace
}  // namespace esphome::aranet::testing
