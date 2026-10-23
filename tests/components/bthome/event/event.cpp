#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "esphome/components/bthome/event/event.h"
#include "esphome/components/bthome/remote_device.h"
#include "esphome/core/application.h"

namespace esphome::bthome::testing {
using namespace esphome::bthome::client;

namespace {

constexpr uint64_t BUTTON_MAC = 0x3C2EF5A1B2C3ULL;
constexpr uint64_t OTHER_MAC = 0x112233445566ULL;
constexpr uint8_t PRESS = 0x01;
constexpr uint8_t DOUBLE_PRESS = 0x02;

ble_device_base::ESPBTDevice make_device(uint64_t address) {
  uint8_t mac_lsb[MAC_ADDRESS_SIZE];
  for (size_t i = 0; i < MAC_ADDRESS_SIZE; i++)
    mac_lsb[i] = static_cast<uint8_t>(address >> (i * 8));
  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac_lsb, -60, ble_device_base::BLE_ADDR_TYPE_PUBLIC, nullptr, 0);
  return device;
}

// One BTHome event entity that records what it fired.
struct RecordingEvent {
  RecordingEvent(BTHomeObjectType type, uint8_t button) {
    this->event.set_object_type(type);
    this->event.set_index(button - 1);
    if (type == BTHomeObjectType::BUTTON_EVENT) {
      this->event.set_event_types({"press", "double_press", "triple_press", "long_press", "long_double_press",
                                   "long_triple_press", "hold_press"});
    } else {
      this->event.set_event_types({"rotate_left", "rotate_right"});
    }
    this->event.add_on_event_callback([this](StringRef type) { this->fired.emplace_back(type.c_str()); });
  }

  BTHomeEvent event;
  std::vector<std::string> fired;
};

// A remote device with the given events, as the hub builds it.
template<size_t N> struct Remote {
  explicit Remote(std::initializer_list<RecordingEvent *> events) {
    this->device.set_address(BUTTON_MAC);
    size_t i = 0;
    for (RecordingEvent *e : events)
      this->device.set_handler(i++, &e->event);
  }

  // Service data starting at the device information byte, received at `now` ms.
  bool receive(std::initializer_list<uint8_t> data, uint32_t now = 0, uint64_t from = BUTTON_MAC) {
    std::vector<uint8_t> bytes(data);
    LoopBlockingGuard clock{nullptr, nullptr, now};
    return this->device.parse_data(make_device(from), bytes.data(), bytes.size());
  }

  RemoteDevice<N> device;
};

using Fired = std::vector<std::string>;

}  // namespace

TEST(BTHomeEvent, FiresOncePerPacketId) {
  RecordingEvent button(BTHomeObjectType::BUTTON_EVENT, 1);
  Remote<1> remote({&button});
  remote.receive({0x44, 0x00, 0x01, 0x3A, PRESS});
  remote.receive({0x44, 0x00, 0x01, 0x3A, PRESS});
  remote.receive({0x44, 0x00, 0x02, 0x3A, DOUBLE_PRESS});
  remote.receive({0x44, 0x00, 0x03, 0x3A, PRESS});
  EXPECT_EQ(button.fired, (Fired{"press", "double_press", "press"}));
}

TEST(BTHomeEvent, ComparesThePacketIdWithThePreviousAdvertisement) {
  RecordingEvent button(BTHomeObjectType::BUTTON_EVENT, 1);
  Remote<1> remote({&button});
  remote.receive({0x44, 0x00, 0x05, 0x3A, PRESS});
  remote.receive({0x44, 0x00, 0x06, 0x3A, DOUBLE_PRESS});
  // The counter is back at 5 after another gesture. That is a new press.
  remote.receive({0x44, 0x00, 0x05, 0x3A, PRESS});
  EXPECT_EQ(button.fired, (Fired{"press", "double_press", "press"}));
}

TEST(BTHomeEvent, SamePacketIdAfterAPauseIsNew) {
  RecordingEvent button(BTHomeObjectType::BUTTON_EVENT, 1);
  Remote<1> remote({&button});
  remote.receive({0x44, 0x00, 0x07, 0x3A, PRESS}, 1000);
  remote.receive({0x44, 0x00, 0x07, 0x3A, PRESS}, 1100);
  // A restarted transmitter sends id 7 again, 4 s after the last copy.
  remote.receive({0x44, 0x00, 0x07, 0x3A, PRESS}, 5100);
  EXPECT_EQ(button.fired, (Fired{"press", "press"}));
}

TEST(BTHomeEvent, DropsCopiesWithoutAPacketId) {
  RecordingEvent button(BTHomeObjectType::BUTTON_EVENT, 1);
  Remote<1> remote({&button});
  remote.receive({0x44, 0x3A, PRESS}, 1000);
  remote.receive({0x44, 0x3A, PRESS}, 1500);
  // 1.2 s after the last copy the same press is a new one.
  remote.receive({0x44, 0x3A, PRESS}, 2700);
  EXPECT_EQ(button.fired, (Fired{"press", "press"}));
}

TEST(BTHomeEvent, MatchesTheMacInThePayload) {
  RecordingEvent button(BTHomeObjectType::BUTTON_EVENT, 1);
  Remote<1> remote({&button});
  // BUTTON_MAC, least significant byte first, behind a changing radio address.
  EXPECT_TRUE(remote.receive({0x46, 0xC3, 0xB2, 0xA1, 0xF5, 0x2E, 0x3C, 0x00, 0x01, 0x3A, PRESS}, 0, OTHER_MAC));
  EXPECT_FALSE(remote.receive({0x46, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0x02, 0x3A, PRESS}, 0, OTHER_MAC));
  EXPECT_EQ(button.fired, (Fired{"press"}));
}

TEST(BTHomeEvent, ReadsItsButtonAndTheHoldAlias) {
  RecordingEvent button2(BTHomeObjectType::BUTTON_EVENT, 2);
  Remote<1> remote({&button2});
  // Only button 1 is in this advertisement.
  remote.receive({0x44, 0x00, 0x01, 0x3A, 0x80});
  // Button 1 has no event, button 2 sends the old hold code 0xFE.
  remote.receive({0x44, 0x00, 0x02, 0x3A, 0x00, 0x3A, 0xFE});
  EXPECT_EQ(button2.fired, (Fired{"hold_press"}));
}

TEST(BTHomeEvent, EachButtonGetsItsOwnObject) {
  // Configured as button 2 first, then button 1: both still see their object.
  RecordingEvent button2(BTHomeObjectType::BUTTON_EVENT, 2);
  RecordingEvent button1(BTHomeObjectType::BUTTON_EVENT, 1);
  Remote<2> remote({&button2, &button1});
  remote.receive({0x44, 0x00, 0x01, 0x3A, PRESS, 0x3A, DOUBLE_PRESS});
  EXPECT_EQ(button1.fired, (Fired{"press"}));
  EXPECT_EQ(button2.fired, (Fired{"double_press"}));
}

TEST(BTHomeEvent, IgnoresNoEventAndUnknownCodes) {
  RecordingEvent button(BTHomeObjectType::BUTTON_EVENT, 1);
  Remote<1> remote({&button});
  remote.receive({0x44, 0x00, 0x01, 0x3A, 0x00});
  remote.receive({0x44, 0x00, 0x02, 0x3A, 0x07});
  EXPECT_TRUE(button.fired.empty());
}

TEST(BTHomeEvent, DimmerFiresTheRotation) {
  RecordingEvent dimmer(BTHomeObjectType::DIMMER_EVENT, 1);
  Remote<1> remote({&dimmer});
  // Rotate left by 3 steps, then right by 1 step.
  remote.receive({0x44, 0x00, 0x01, 0x3C, 0x01, 0x03});
  remote.receive({0x44, 0x00, 0x02, 0x3C, 0x02, 0x01});
  EXPECT_EQ(dimmer.fired, (Fired{"rotate_left", "rotate_right"}));
}

}  // namespace esphome::bthome::testing
