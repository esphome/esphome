#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "../common.h"

namespace esphome::bthome_mithermometer::testing {

namespace {

// Encrypted BTHome v2 frames for SENSOR_ADDRESS, generated with Python `cryptography`
// AESCCM(tag_length=4) and the bindkey below. Layout: device info (0x41), ciphertext,
// counter (little-endian), MIC. Each plaintext is a single temperature object (0x02).
// Bindkey: eef418daf699a0c188f3bfd17e4565d9
using Frame = std::array<uint8_t, 12>;

// Counter 10, 22.50 C
constexpr Frame COUNTER_10 = {0x41, 0xB9, 0xBD, 0x25, 0x0A, 0x00, 0x00, 0x00, 0x47, 0x51, 0x79, 0x04};
// Counter 11, 23.48 C
constexpr Frame COUNTER_11 = {0x41, 0xD8, 0x9D, 0x54, 0x0B, 0x00, 0x00, 0x00, 0x4F, 0x5D, 0x89, 0xF2};
// Counter 11 again, 4.00 C
constexpr Frame COUNTER_11_OTHER = {0x41, 0xD8, 0x21, 0x5C, 0x0B, 0x00, 0x00, 0x00, 0x72, 0xF8, 0x2B, 0x55};
// Counter 9, 4.00 C
constexpr Frame COUNTER_9 = {0x41, 0x17, 0xB6, 0x8D, 0x09, 0x00, 0x00, 0x00, 0xFB, 0x42, 0xC4, 0x2D};

// Counter 100 from another device sharing the bindkey (address SENSOR_ADDRESS + 1), 30.00 C
constexpr uint64_t OTHER_ADDRESS = SENSOR_ADDRESS + 1;
constexpr Frame OTHER_COUNTER_100 = {0x41, 0x48, 0xD7, 0xD1, 0x64, 0x00, 0x00, 0x00, 0x09, 0x5B, 0xEC, 0x22};

struct Harness {
  explicit Harness(bool replay_protection = true) {
    this->thermometer.set_address(SENSOR_ADDRESS);
    this->thermometer.set_bindkey(
        {0xEE, 0xF4, 0x18, 0xDA, 0xF6, 0x99, 0xA0, 0xC1, 0x88, 0xF3, 0xBF, 0xD1, 0x7E, 0x45, 0x65, 0xD9});
    this->thermometer.set_replay_protection(replay_protection);
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

TEST(BTHomeMiThermometerReplay, DropsExactRepeat) {
  Harness h;
  ASSERT_TRUE(h.thermometer.parse_device(advert(COUNTER_11)));
  EXPECT_FALSE(h.thermometer.parse_device(advert(COUNTER_11)));
}

// Some firmwares (e.g. PVVX) send new data under the same counter.
TEST(BTHomeMiThermometerReplay, AcceptsNewDataWithSameCounter) {
  Harness h;
  ASSERT_TRUE(h.thermometer.parse_device(advert(COUNTER_11)));
  ASSERT_TRUE(h.thermometer.parse_device(advert(COUNTER_11_OTHER)));
  EXPECT_NEAR(h.temperature.state, 4.0f, 0.001f);
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

// Without replay_protection the counter is not checked at all.
TEST(BTHomeMiThermometerReplay, DisabledAcceptsLowerCounter) {
  Harness h(false);
  ASSERT_TRUE(h.thermometer.parse_device(advert(COUNTER_10)));
  ASSERT_TRUE(h.thermometer.parse_device(advert(COUNTER_9)));
  EXPECT_NEAR(h.temperature.state, 4.0f, 0.001f);
}

// A frame from another device that decrypts with the same bindkey is rejected
// by the address check and must not move the replay counter.
TEST(BTHomeMiThermometerReplay, OtherDeviceDoesNotAffectCounter) {
  Harness h;
  EXPECT_FALSE(h.thermometer.parse_device(advert(OTHER_COUNTER_100, OTHER_ADDRESS)));
  ASSERT_TRUE(h.thermometer.parse_device(advert(COUNTER_10)));
  EXPECT_NEAR(h.temperature.state, 22.5f, 0.001f);
}

}  // namespace esphome::bthome_mithermometer::testing
