#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "../common.h"
#include "esphome/components/aranet4/aranet4.h"

namespace esphome::aranet4::testing {
namespace {

ble_device_base::ESPBTDevice make_device(const std::vector<uint8_t> &payload, int rssi = -61) {
  std::vector<uint8_t> advertisement{static_cast<uint8_t>(payload.size() + 3), 0xFF, 0x02, 0x07};
  advertisement.insert(advertisement.end(), payload.begin(), payload.end());

  ble_device_base::ESPBTDevice device;
  device.from_scan_result(MAC_ADDRESS, rssi, 0, advertisement.data(), advertisement.size());
  return device;
}

TEST(Aranet4, DecodesManufacturerData) {
  auto payload = make_payload();
  auto device = make_device({payload.begin(), payload.end()});
  Aranet4 aranet(device.address_uint64());
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

TEST(Aranet4, UpdatesDiagnosticsForRepeatedMeasurement) {
  auto payload = make_payload();
  auto first_device = make_device({payload.begin(), payload.end()}, -61);
  Aranet4 aranet(first_device.address_uint64());
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

TEST(Aranet4, RejectsDisabledIntegrationAndMalformedPayload) {
  const auto valid_payload = make_payload();
  std::vector<uint8_t> payload(valid_payload.begin(), valid_payload.end());
  auto valid_device = make_device({payload.begin(), payload.end()});
  Aranet4 aranet(valid_device.address_uint64());

  payload[0] = 0;
  EXPECT_FALSE(aranet.parse_device(make_device({payload.begin(), payload.end()})));

  payload.pop_back();
  EXPECT_FALSE(aranet.parse_device(make_device({payload.begin(), payload.end()})));
}

}  // namespace
}  // namespace esphome::aranet4::testing
