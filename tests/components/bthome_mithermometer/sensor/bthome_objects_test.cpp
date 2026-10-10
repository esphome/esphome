#include <gtest/gtest.h>

#include "../common.h"

namespace esphome::bthome_mithermometer::testing {

namespace {

// Service data in these tests is unencrypted BTHome v2.
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
