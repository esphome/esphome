#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "esphome/components/bthome_mithermometer/bthome_ble.h"

namespace esphome::bthome_mithermometer::testing {

namespace {

constexpr uint64_t SENSOR_ADDRESS = 0xA4C1384E1678ULL;

// Encrypted BTHome v2 frames for SENSOR_ADDRESS, generated with Python `cryptography`
// AESCCM(tag_length=4) and the bindkey below. Layout: device info (0x41), ciphertext,
// counter (little-endian), MIC. Each plaintext is a single temperature object (0x02).
constexpr std::initializer_list<uint8_t> BINDKEY = {0xEE, 0xF4, 0x18, 0xDA, 0xF6, 0x99, 0xA0, 0xC1,
                                                    0x88, 0xF3, 0xBF, 0xD1, 0x7E, 0x45, 0x65, 0xD9};
// Counter 10, 22.50 C
const std::vector<uint8_t> COUNTER_10 = {0x41, 0xB9, 0xBD, 0x25, 0x0A, 0x00, 0x00, 0x00, 0x47, 0x51, 0x79, 0x04};
// Counter 11, 23.48 C
const std::vector<uint8_t> COUNTER_11 = {0x41, 0xD8, 0x9D, 0x54, 0x0B, 0x00, 0x00, 0x00, 0x4F, 0x5D, 0x89, 0xF2};
// Counter 11 again, 4.00 C
const std::vector<uint8_t> COUNTER_11_OTHER = {0x41, 0xD8, 0x21, 0x5C, 0x0B, 0x00,
                                               0x00, 0x00, 0x72, 0xF8, 0x2B, 0x55};
// Counter 9, 4.00 C
const std::vector<uint8_t> COUNTER_9 = {0x41, 0x17, 0xB6, 0x8D, 0x09, 0x00, 0x00, 0x00, 0xFB, 0x42, 0xC4, 0x2D};

ble_device_base::ESPBTDevice advert(const std::vector<uint8_t> &service_data) {
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
    this->thermometer.set_bindkey(BINDKEY);
    this->thermometer.set_temperature(&this->temperature);
  }

  BTHomeMiThermometer thermometer;
  sensor::Sensor temperature;
};

}  // namespace

TEST(BTHomeMiThermometerReplay, AcceptsIncreasingCounters) {
  Harness h;
  ASSERT_TRUE(h.thermometer.parse_device(advert(COUNTER_10)));
  EXPECT_NEAR(h.temperature.state, 22.5f, 0.001f);
  ASSERT_TRUE(h.thermometer.parse_device(advert(COUNTER_11)));
  EXPECT_NEAR(h.temperature.state, 23.48f, 0.001f);
}

TEST(BTHomeMiThermometerReplay, DropsRepeatedCounter) {
  Harness h;
  ASSERT_TRUE(h.thermometer.parse_device(advert(COUNTER_11)));
  EXPECT_FALSE(h.thermometer.parse_device(advert(COUNTER_11_OTHER)));
  EXPECT_NEAR(h.temperature.state, 23.48f, 0.001f);
}

TEST(BTHomeMiThermometerReplay, RejectsLowerCounter) {
  Harness h;
  ASSERT_TRUE(h.thermometer.parse_device(advert(COUNTER_10)));
  EXPECT_FALSE(h.thermometer.parse_device(advert(COUNTER_9)));
  EXPECT_NEAR(h.temperature.state, 22.5f, 0.001f);
}

// The first frame after boot has nothing to compare against and is accepted.
TEST(BTHomeMiThermometerReplay, AcceptsFirstFrameWithAnyCounter) {
  Harness h;
  ASSERT_TRUE(h.thermometer.parse_device(advert(COUNTER_9)));
  EXPECT_NEAR(h.temperature.state, 4.0f, 0.001f);
}

}  // namespace esphome::bthome_mithermometer::testing
