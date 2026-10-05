#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "esphome/components/bthome/bthome_binary_sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::bthome::testing {

namespace {

constexpr uint64_t BUTTON_ADDRESS = 0x3C2EF5A1B2C3ULL;
constexpr uint64_t OTHER_ADDRESS = 0x112233445566ULL;
constexpr uint8_t PRESS = 0x01;
constexpr uint8_t DOUBLE_PRESS = 0x02;

// Service data for a 16-bit UUID (BTHome by default), received from `address`.
ble_device_base::ESPBTDevice advert(uint64_t address, std::initializer_list<uint8_t> service_data,
                                    uint16_t uuid = 0xFCD2) {
  std::vector<uint8_t> adv = {static_cast<uint8_t>(service_data.size() + 3), 0x16, static_cast<uint8_t>(uuid & 0xFF),
                              static_cast<uint8_t>(uuid >> 8)};
  adv.insert(adv.end(), service_data.begin(), service_data.end());
  uint8_t mac[6];
  for (size_t i = 0; i < 6; i++)
    mac[i] = static_cast<uint8_t>(address >> (i * 8));
  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac, -60, 0, adv.data(), static_cast<uint16_t>(adv.size()));
  return device;
}

struct Harness {
  explicit Harness(uint8_t event, uint8_t index = 1) {
    // The test main does not construct App as generated code does; the pulse needs its scheduler.
    static const bool APP_CONSTRUCTED = (new (&App) Application(), true);
    (void) APP_CONSTRUCTED;
    this->button.set_address(BUTTON_ADDRESS);
    this->button.set_event(event);
    this->button.set_index(index);
    this->button.set_pulse_length(0);
    this->button.setup();
    // One dispatch sets App's loop time to millis(), as a running loop would have.
    App.scheduler.set_timeout(&this->button, "sync", 0, []() {});
    App.scheduler.call(millis());
  }

  ~Harness() {
    App.scheduler.cancel_timeout(&this->button, "pulse");
    App.scheduler.call(millis());
  }

  // True when the advertisement turned the sensor on. Runs the pulse to its end afterwards.
  bool fires(const ble_device_base::ESPBTDevice &device) {
    this->button.parse_device(device);
    const bool on = this->button.state;
    App.scheduler.call(millis());
    EXPECT_FALSE(this->button.state);
    return on;
  }

  BTHomeButtonBinarySensor button;
};

}  // namespace

TEST(BTHomeButtonBinarySensor, FiresOncePerPacketIdForItsEvent) {
  Harness h(PRESS);
  EXPECT_FALSE(h.button.state);
  EXPECT_TRUE(h.fires(advert(BUTTON_ADDRESS, {0x44, 0x00, 0x01, 0x3A, PRESS})));
  EXPECT_FALSE(h.fires(advert(BUTTON_ADDRESS, {0x44, 0x00, 0x01, 0x3A, PRESS})));
  EXPECT_FALSE(h.fires(advert(BUTTON_ADDRESS, {0x44, 0x00, 0x02, 0x3A, DOUBLE_PRESS})));
  EXPECT_TRUE(h.fires(advert(BUTTON_ADDRESS, {0x44, 0x00, 0x03, 0x3A, PRESS})));
}

TEST(BTHomeButtonBinarySensor, ComparesThePacketIdWithThePreviousAdvertisement) {
  Harness h(PRESS);
  EXPECT_TRUE(h.fires(advert(BUTTON_ADDRESS, {0x44, 0x00, 0x05, 0x3A, PRESS})));
  EXPECT_FALSE(h.fires(advert(BUTTON_ADDRESS, {0x44, 0x00, 0x06, 0x3A, DOUBLE_PRESS})));
  // The counter is back at 5 after other gestures. That is a new press.
  EXPECT_TRUE(h.fires(advert(BUTTON_ADDRESS, {0x44, 0x00, 0x05, 0x3A, PRESS})));
}

TEST(BTHomeButtonBinarySensor, MatchesTheMacInThePayload) {
  Harness h(PRESS);
  // BUTTON_ADDRESS, least significant byte first, behind a changing radio address.
  EXPECT_TRUE(h.fires(advert(OTHER_ADDRESS, {0x46, 0xC3, 0xB2, 0xA1, 0xF5, 0x2E, 0x3C, 0x00, 0x01, 0x3A, PRESS})));
  EXPECT_FALSE(h.fires(advert(OTHER_ADDRESS, {0x46, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0x02, 0x3A, PRESS})));
}

TEST(BTHomeButtonBinarySensor, ReadsItsIndexAndTheHoldAlias) {
  Harness h(0x80, 2);
  EXPECT_FALSE(h.fires(advert(BUTTON_ADDRESS, {0x44, 0x00, 0x01, 0x3A, 0x80})));
  EXPECT_TRUE(h.fires(advert(BUTTON_ADDRESS, {0x44, 0x00, 0x02, 0x3A, 0x00, 0x3A, 0xFE})));
}

TEST(BTHomeButtonBinarySensor, DropsCopiesWithoutAPacketId) {
  Harness h(PRESS);
  EXPECT_TRUE(h.fires(advert(BUTTON_ADDRESS, {0x44, 0x3A, PRESS})));
  EXPECT_FALSE(h.fires(advert(BUTTON_ADDRESS, {0x44, 0x3A, PRESS})));
}

TEST(BTHomeButtonBinarySensor, IgnoresEncryptedForeignAndOtherServiceData) {
  Harness h(PRESS);
  const auto encrypted = advert(BUTTON_ADDRESS, {0x45, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99});
  EXPECT_TRUE(h.button.parse_device(encrypted));
  EXPECT_TRUE(h.button.parse_device(encrypted));
  EXPECT_FALSE(h.button.state);

  EXPECT_FALSE(h.button.parse_device(advert(OTHER_ADDRESS, {0x44, 0x00, 0x01, 0x3A, PRESS})));
  EXPECT_FALSE(h.button.parse_device(advert(BUTTON_ADDRESS, {0x44, 0x00, 0x01, 0x3A, PRESS}, 0xFE95)));
  EXPECT_FALSE(h.button.state);
}

}  // namespace esphome::bthome::testing
