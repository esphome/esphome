#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "esphome/components/bthome_mithermometer/bthome_ble.h"

namespace esphome::bthome_mithermometer::testing {

namespace {

constexpr uint64_t SENSOR_ADDRESS = 0xA4C1384E1678ULL;

// Unencrypted BTHome v2 service data (UUID 0xFCD2) sent from SENSOR_ADDRESS.
ble_device_base::ESPBTDevice advert(std::initializer_list<uint8_t> service_data) {
  std::vector<uint8_t> adv = {static_cast<uint8_t>(service_data.size() + 3), 0x16, 0xD2, 0xFC};
  adv.insert(adv.end(), service_data.begin(), service_data.end());
  uint8_t mac[6];
  for (size_t i = 0; i < 6; i++)
    mac[i] = static_cast<uint8_t>(SENSOR_ADDRESS >> (i * 8));
  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac, -60, 0, adv.data(), static_cast<uint16_t>(adv.size()));
  return device;
}

struct Harness {
  Harness() {
    this->thermometer.set_address(SENSOR_ADDRESS);
    this->thermometer.set_temperature(&this->temperature);
    this->thermometer.set_humidity(&this->humidity);
    this->thermometer.set_battery_level(&this->battery_level);
  }

  BTHomeMiThermometer thermometer;
  sensor::Sensor temperature, humidity, battery_level;
};

}  // namespace

// Shelly BLU H&T: packet id, battery 90 %, humidity 55 % (0x2E), temperature 22.5 C (0x45)
TEST(BTHomeMiThermometerObjects, DecodesShellyStyleTemperatureAndHumidity) {
  Harness h;
  ASSERT_TRUE(h.thermometer.parse_device(advert({0x40, 0x00, 0x01, 0x01, 0x5A, 0x2E, 0x37, 0x45, 0xE1, 0x00})));
  EXPECT_NEAR(h.temperature.state, 22.5f, 0.001f);
  EXPECT_FLOAT_EQ(h.humidity.state, 55.0f);
  EXPECT_FLOAT_EQ(h.battery_level.state, 90.0f);
}

TEST(BTHomeMiThermometerObjects, DecodesNegativeTenthDegreeTemperature) {
  Harness h;
  // -5.3 C = -53 = 0xFFCB
  ASSERT_TRUE(h.thermometer.parse_device(advert({0x40, 0x45, 0xCB, 0xFF})));
  EXPECT_NEAR(h.temperature.state, -5.3f, 0.001f);
  EXPECT_FALSE(h.humidity.has_state());
}

// PVVX firmware: temperature 22.50 C (0x02) and humidity 45.67 % (0x03) keep working
TEST(BTHomeMiThermometerObjects, StillDecodesHundredthDegreeObjects) {
  Harness h;
  ASSERT_TRUE(h.thermometer.parse_device(advert({0x40, 0x02, 0xCA, 0x08, 0x03, 0xD7, 0x11})));
  EXPECT_NEAR(h.temperature.state, 22.5f, 0.001f);
  EXPECT_NEAR(h.humidity.state, 45.67f, 0.001f);
}

}  // namespace esphome::bthome_mithermometer::testing
