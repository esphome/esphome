#include <gtest/gtest.h>
#include "esphome/components/bthome/remote_device.h"
#include "esphome/components/bthome/remote_listener.h"
#include "esphome/core/helpers.h"

namespace esphome::bthome::testing {
using namespace esphome::bthome::client;
// Handler that records how many times it was called for a specific object type
class MockHandler : public BTHomeRemoteObject {
 public:
  explicit MockHandler(BTHomeObjectType expected_type) : expected_type_(expected_type) {}

  bool process_object(const BTHomeObject &object) override {
    if (object.type != this->expected_type_)
      return false;
    this->call_count_++;
    return true;
  }

  int call_count() const { return this->call_count_; }

 private:
  BTHomeObjectType expected_type_;
  int call_count_{0};
};

// Minimal valid BTHome payload: header (v2, unencrypted=0x40) + BATTERY_PCT (0x01) + 97% (0x61)
static const uint8_t kBatteryPayload[] = {0x40, 0x01, 0x61};

static const MacAddress kMacA{0x010203040506ULL};
static const MacAddress kMacB{0xAABBCCDDEEFFULL};

class DeviceListenerTest : public ::testing::Test {
 protected:
  DeviceListener<2> listener_;
  RemoteDevice<1> device_a_;
  RemoteDevice<1> device_b_;
  MockHandler handler_a_{BTHomeObjectType::BATTERY_PCT};
  MockHandler handler_b_{BTHomeObjectType::BATTERY_PCT};

  void SetUp() override {
    this->device_a_.set_address(kMacA);
    this->device_a_.set_handler(0, &this->handler_a_);
    this->device_b_.set_address(kMacB);
    this->device_b_.set_handler(0, &this->handler_b_);
    this->listener_.set_device(0, &this->device_a_);
    this->listener_.set_device(1, &this->device_b_);
  }
};

// on_bthome_data routes to the device whose MAC matches the source address
TEST_F(DeviceListenerTest, RoutesToMatchingDevice) {
  EXPECT_TRUE(this->listener_.on_bthome_data(kMacA, kBatteryPayload, sizeof(kBatteryPayload)));
  EXPECT_EQ(this->handler_a_.call_count(), 1);
  EXPECT_EQ(this->handler_b_.call_count(), 0);
}

// Returns false when no registered device has the source MAC address
TEST_F(DeviceListenerTest, ReturnsFalseWhenNoDeviceMatches) {
  MacAddress unknown_mac{0x111111111111ULL};
  EXPECT_FALSE(this->listener_.on_bthome_data(unknown_mac, kBatteryPayload, sizeof(kBatteryPayload)));
  EXPECT_EQ(this->handler_a_.call_count(), 0);
  EXPECT_EQ(this->handler_b_.call_count(), 0);
}

// Tries subsequent devices when an earlier one does not match the source MAC
TEST_F(DeviceListenerTest, SecondDeviceCalledWhenFirstDoesNotMatch) {
  EXPECT_TRUE(this->listener_.on_bthome_data(kMacB, kBatteryPayload, sizeof(kBatteryPayload)));
  EXPECT_EQ(this->handler_a_.call_count(), 0);
  EXPECT_EQ(this->handler_b_.call_count(), 1);
}

// Stops dispatching after the first device claims the data (returns true)
TEST_F(DeviceListenerTest, StopsAfterFirstMatch) {
  // Give device_b_ the same MAC as device_a_ so both would match
  this->device_b_.set_address(kMacA);

  EXPECT_TRUE(this->listener_.on_bthome_data(kMacA, kBatteryPayload, sizeof(kBatteryPayload)));
  EXPECT_EQ(this->handler_a_.call_count(), 1);
  EXPECT_EQ(this->handler_b_.call_count(), 0);  // never reached
}

// Null device slots (unset entries in the array) are skipped without crashing
TEST_F(DeviceListenerTest, NullDeviceSlotSkipped) {
  DeviceListener<2> listener_with_null;
  listener_with_null.set_device(0, nullptr);
  listener_with_null.set_device(1, &this->device_a_);

  EXPECT_TRUE(listener_with_null.on_bthome_data(kMacA, kBatteryPayload, sizeof(kBatteryPayload)));
  EXPECT_EQ(this->handler_a_.call_count(), 1);
}

}  // namespace esphome::bthome::testing
