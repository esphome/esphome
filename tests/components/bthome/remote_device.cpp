#include <gtest/gtest.h>
#include "esphome/components/bthome/remote_device.h"
#include "esphome/core/helpers.h"

namespace esphome::bthome::testing {
using namespace esphome::bthome::client;

// Mock handler that tracks processed objects
class MockBTHomeRemoteObject : public BTHomeRemoteObject {
 public:
  struct ProcessedObject {
    BTHomeObjectType type;
    float value;
  };

  MockBTHomeRemoteObject(BTHomeObjectType expected_type) : expected_type_(expected_type) {}

  bool process_object(const BTHomeObject &object) override {
    if (object.type != this->expected_type_)
      return false;

    ProcessedObject processed{
        .type = object.type,
        .value = object.as_float(),
    };
    processed_objects_.push_back(processed);
    return true;
  }

  const std::vector<ProcessedObject> &processed_objects() const { return processed_objects_; }

 private:
  BTHomeObjectType expected_type_;
  std::vector<ProcessedObject> processed_objects_;
};

class BTHomeDeviceTest : public ::testing::Test {
 protected:
  // Create a device with 2 handlers
  RemoteDevice<2> device;
  MockBTHomeRemoteObject handler1{BTHomeObjectType::BATTERY_PCT};
  MockBTHomeRemoteObject handler2{BTHomeObjectType::TEMPERATURE_C_E2};

  // Test MAC address: 01:02:03:04:05:06
  MacAddress test_mac_{0x010203040506ULL};
  // Different MAC address: AA:BB:CC:DD:EE:FF
  MacAddress other_mac_{0xAABBCCDDEEFF};

  void SetUp() override {
    // Set the device address
    device.set_address(test_mac_);
    // Register handlers
    device.set_handler(0, &handler1);
    device.set_handler(1, &handler2);
  }
};

TEST_F(BTHomeDeviceTest, ParseDataWithMatchingMacAddress) {
  // Create unencrypted BTHome payload with BATTERY_PCT (97%)
  // Header (unencrypted, version 2): 0x40
  // BATTERY_PCT + data: 0x01 0x61
  uint8_t payload[] = {0x40, 0x01, 0x61};

  bool result = device.parse_data(test_mac_, payload, sizeof(payload));

  EXPECT_TRUE(result);
  EXPECT_EQ(handler1.processed_objects().size(), 1);
  EXPECT_EQ(handler2.processed_objects().size(), 0);
  EXPECT_NEAR(handler1.processed_objects()[0].value, 97.0f, 0.001f);
}

TEST_F(BTHomeDeviceTest, ParseDataWithNonMatchingMacAddress) {
  // Create unencrypted BTHome payload
  uint8_t payload[] = {0x40, 0x01, 0x61};

  bool result = device.parse_data(MacAddressPtr(other_mac_), payload, sizeof(payload));

  // Should return false when MAC address doesn't match
  EXPECT_FALSE(result);
  // Handlers should not be called
  EXPECT_EQ(handler1.processed_objects().size(), 0);
  EXPECT_EQ(handler2.processed_objects().size(), 0);
}

TEST_F(BTHomeDeviceTest, ParseDataMultipleObjects) {
  // Create unencrypted BTHome payload with BATTERY_PCT and TEMPERATURE_C_E2
  // Header (version 2): 0x40
  // BATTERY_PCT (97%): 0x01 0x61
  // TEMPERATURE_C_E2 (25.06°C): 0x02 0xCA 0x09
  uint8_t payload[] = {0x40, 0x01, 0x61, 0x02, 0xCA, 0x09};

  bool result = device.parse_data(test_mac_, payload, sizeof(payload));

  EXPECT_TRUE(result);
  EXPECT_EQ(handler1.processed_objects().size(), 1);
  EXPECT_EQ(handler2.processed_objects().size(), 1);
  EXPECT_NEAR(handler1.processed_objects()[0].value, 97.0f, 0.001f);
  EXPECT_NEAR(handler2.processed_objects()[0].value, 25.06f, 0.001f);
}

TEST_F(BTHomeDeviceTest, ParseDataWithPacketId) {
  // Create payload with PACKET_ID (0x00) followed by BATTERY_PCT
  // Header (version 2): 0x40
  // PACKET_ID: 0x00 0x42 (packet ID = 66)
  // BATTERY_PCT: 0x01 0x61
  uint8_t payload[] = {0x40, 0x00, 0x42, 0x01, 0x61};

  bool result = device.parse_data(test_mac_, payload, sizeof(payload));

  EXPECT_TRUE(result);
  EXPECT_EQ(handler1.processed_objects().size(), 1);
}

TEST_F(BTHomeDeviceTest, ParseDataDuplicatePacketId) {
  // Create payload with PACKET_ID 0x42
  uint8_t payload[] = {0x40, 0x00, 0x42, 0x01, 0x61};

  // First parse should succeed
  bool result1 = device.parse_data(test_mac_, payload, sizeof(payload));
  EXPECT_TRUE(result1);
  EXPECT_EQ(handler1.processed_objects().size(), 1);

  // Second parse with same packet ID should be ignored
  bool result2 = device.parse_data(test_mac_, payload, sizeof(payload));
  EXPECT_TRUE(result2);
  // Handler should not have been called again
  EXPECT_EQ(handler1.processed_objects().size(), 1);
}

TEST_F(BTHomeDeviceTest, ParseDataDifferentPacketId) {
  // First payload with PACKET_ID 0x42
  uint8_t payload1[] = {0x40, 0x00, 0x42, 0x01, 0x61};
  bool result1 = device.parse_data(test_mac_, payload1, sizeof(payload1));
  EXPECT_TRUE(result1);
  EXPECT_EQ(handler1.processed_objects().size(), 1);

  // Second payload with PACKET_ID 0x43 (different ID)
  uint8_t payload2[] = {0x40, 0x00, 0x43, 0x01, 0x32};
  bool result2 = device.parse_data(test_mac_, payload2, sizeof(payload2));
  EXPECT_TRUE(result2);
  // Handler should be called again because packet ID is different
  EXPECT_EQ(handler1.processed_objects().size(), 2);
  EXPECT_NEAR(handler1.processed_objects()[1].value, 50.0f, 0.001f);
}

TEST_F(BTHomeDeviceTest, ParseDataNoMatchingHandler) {
  // Create payload with an object type that doesn't match any handler
  // HUMIDITY_PCT_E2 (0x03) - neither handler expects this
  // Header (version 2): 0x40
  // HUMIDITY_PCT_E2: 0x03 0xBF 0x13
  uint8_t payload[] = {0x40, 0x03, 0xBF, 0x13};

  bool result = device.parse_data(test_mac_, payload, sizeof(payload));

  EXPECT_TRUE(result);
  // No handler should have processed the object
  EXPECT_EQ(handler1.processed_objects().size(), 0);
  EXPECT_EQ(handler2.processed_objects().size(), 0);
}

TEST_F(BTHomeDeviceTest, ParseDataOnlyUnmatchedObjectsAfterMatch) {
  // Create payload with BATTERY_PCT followed by unmatched object type
  // Header (version 2): 0x40
  // BATTERY_PCT: 0x01 0x61
  // HUMIDITY_PCT_E2: 0x03 0xBF 0x13
  uint8_t payload[] = {0x40, 0x01, 0x61, 0x03, 0xBF, 0x13};

  bool result = device.parse_data(test_mac_, payload, sizeof(payload));

  EXPECT_TRUE(result);
  EXPECT_EQ(handler1.processed_objects().size(), 1);
  EXPECT_EQ(handler2.processed_objects().size(), 0);
}

TEST_F(BTHomeDeviceTest, ParseDataRepeatedObjectType) {
  // Test with repeated object types - each handler gets its own reading
  // Create a device with 3 handlers: TEMP, TEMP, BATTERY
  RemoteDevice<3> device3;
  MockBTHomeRemoteObject temp_handler1{BTHomeObjectType::TEMPERATURE_C_E2};
  MockBTHomeRemoteObject temp_handler2{BTHomeObjectType::TEMPERATURE_C_E2};
  MockBTHomeRemoteObject batt_handler{BTHomeObjectType::BATTERY_PCT};

  device3.set_address(MacAddress(test_mac_));
  device3.set_handler(0, &temp_handler1);
  device3.set_handler(1, &temp_handler2);
  device3.set_handler(2, &batt_handler);

  // Payload with two TEMPERATURE_C_E2 objects and one BATTERY_PCT
  // Header: 0x40
  // TEMPERATURE_C_E2 (25.06°C): 0x02 0xCA 0x09
  // TEMPERATURE_C_E2 (47.23°C): 0x02 0x73 0x12
  // BATTERY_PCT (97%): 0x01 0x61
  uint8_t payload[] = {0x40, 0x02, 0xCA, 0x09, 0x02, 0x73, 0x12, 0x01, 0x61};

  bool result = device3.parse_data(test_mac_, payload, sizeof(payload));

  EXPECT_TRUE(result);
  // First temperature handler should get the first reading
  EXPECT_EQ(temp_handler1.processed_objects().size(), 1);
  EXPECT_NEAR(temp_handler1.processed_objects()[0].value, 25.06f, 0.001f);
  // Second temperature handler should get the second reading
  EXPECT_EQ(temp_handler2.processed_objects().size(), 1);
  EXPECT_NEAR(temp_handler2.processed_objects()[0].value, 47.23f, 0.001f);
  // Battery handler should get its reading
  EXPECT_EQ(batt_handler.processed_objects().size(), 1);
  EXPECT_NEAR(batt_handler.processed_objects()[0].value, 97.0f, 0.001f);
}

}  // namespace esphome::bthome::testing
