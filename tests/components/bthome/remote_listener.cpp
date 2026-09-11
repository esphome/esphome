#include <gtest/gtest.h>
#include "esphome/components/bthome/remote_device.h"
#include "esphome/components/bthome/remote_listener.h"
#include "esphome/components/ble_device_base/ble_device.h"
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

static const MacAddress K_MAC_A{0x010203040506ULL};
static const MacAddress K_MAC_B{0xAABBCCDDEEFFULL};
static const uint8_t K_MAC_A_LSB[] = {0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
static const uint8_t K_MAC_B_LSB[] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA};

static ble_device_base::ESPBTDevice make_device(const uint8_t *mac, const uint8_t *advertisement, size_t size) {
  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac, -50, ble_device_base::BLE_ADDR_TYPE_PUBLIC, advertisement, static_cast<uint16_t>(size));
  return device;
}

class DeviceListenerTest : public ::testing::Test {
 protected:
  DeviceListener<2> listener_;
  RemoteDevice<1> device_a_;
  RemoteDevice<1> device_b_;
  MockHandler handler_a_{BTHomeObjectType::BATTERY_PCT};
  MockHandler handler_b_{BTHomeObjectType::BATTERY_PCT};

  void SetUp() override {
    this->device_a_.set_address(K_MAC_A);
    this->device_a_.set_handler(0, &this->handler_a_);
    this->device_b_.set_address(K_MAC_B);
    this->device_b_.set_handler(0, &this->handler_b_);
    this->listener_.set_device(0, &this->device_a_);
    this->listener_.set_device(1, &this->device_b_);
  }
};

// on_bthome_data routes to the device whose MAC matches the source address
TEST_F(DeviceListenerTest, RoutesToMatchingDevice) {
  const uint8_t advertisement[] = {0x06, 0x16, 0xD2, 0xFC, 0x40, 0x01, 0x61};
  EXPECT_TRUE(this->listener_.parse_device(make_device(K_MAC_A_LSB, advertisement, sizeof(advertisement))));
  EXPECT_EQ(this->handler_a_.call_count(), 1);
  EXPECT_EQ(this->handler_b_.call_count(), 0);
}

TEST_F(DeviceListenerTest, IgnoresWrongServiceUuid) {
  const uint8_t advertisement[] = {0x06, 0x16, 0xD3, 0xFC, 0x40, 0x01, 0x61};
  EXPECT_FALSE(this->listener_.parse_device(make_device(K_MAC_A_LSB, advertisement, sizeof(advertisement))));
  EXPECT_EQ(this->handler_a_.call_count(), 0);
  EXPECT_EQ(this->handler_b_.call_count(), 0);
}

TEST_F(DeviceListenerTest, IgnoresEmptyServiceData) {
  const uint8_t advertisement[] = {0x03, 0x16, 0xD2, 0xFC};
  EXPECT_FALSE(this->listener_.parse_device(make_device(K_MAC_A_LSB, advertisement, sizeof(advertisement))));
  EXPECT_EQ(this->handler_a_.call_count(), 0);
  EXPECT_EQ(this->handler_b_.call_count(), 0);
}

TEST_F(DeviceListenerTest, IgnoresUnsupportedVersion) {
  const uint8_t advertisement[] = {0x06, 0x16, 0xD2, 0xFC, 0x20, 0x01, 0x61};
  EXPECT_FALSE(this->listener_.parse_device(make_device(K_MAC_A_LSB, advertisement, sizeof(advertisement))));
  EXPECT_EQ(this->handler_a_.call_count(), 0);
  EXPECT_EQ(this->handler_b_.call_count(), 0);
}

TEST_F(DeviceListenerTest, ReturnsFalseWhenNoDeviceMatches) {
  const uint8_t mac_lsb[] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
  const uint8_t advertisement[] = {0x06, 0x16, 0xD2, 0xFC, 0x40, 0x01, 0x61};
  EXPECT_FALSE(this->listener_.parse_device(make_device(mac_lsb, advertisement, sizeof(advertisement))));
  EXPECT_EQ(this->handler_a_.call_count(), 0);
  EXPECT_EQ(this->handler_b_.call_count(), 0);
}

TEST_F(DeviceListenerTest, RoutesToSecondMatchingDevice) {
  const uint8_t advertisement[] = {0x06, 0x16, 0xD2, 0xFC, 0x40, 0x01, 0x61};
  EXPECT_TRUE(this->listener_.parse_device(make_device(K_MAC_B_LSB, advertisement, sizeof(advertisement))));
  EXPECT_EQ(this->handler_a_.call_count(), 0);
  EXPECT_EQ(this->handler_b_.call_count(), 1);
}

TEST_F(DeviceListenerTest, StopsAfterFirstMatchingDevice) {
  this->device_b_.set_address(K_MAC_A);
  const uint8_t advertisement[] = {0x06, 0x16, 0xD2, 0xFC, 0x40, 0x01, 0x61};
  EXPECT_TRUE(this->listener_.parse_device(make_device(K_MAC_A_LSB, advertisement, sizeof(advertisement))));
  EXPECT_EQ(this->handler_a_.call_count(), 1);
  EXPECT_EQ(this->handler_b_.call_count(), 0);
}

TEST_F(DeviceListenerTest, SkipsNullDeviceSlot) {
  DeviceListener<2> listener_with_null;
  listener_with_null.set_device(0, nullptr);
  listener_with_null.set_device(1, &this->device_a_);
  const uint8_t advertisement[] = {0x06, 0x16, 0xD2, 0xFC, 0x40, 0x01, 0x61};
  EXPECT_TRUE(listener_with_null.parse_device(make_device(K_MAC_A_LSB, advertisement, sizeof(advertisement))));
  EXPECT_EQ(this->handler_a_.call_count(), 1);
}

}  // namespace esphome::bthome::testing
