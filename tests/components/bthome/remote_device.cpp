#include <gtest/gtest.h>
#include "esphome/components/bthome/remote_device.h"
#include "esphome/core/helpers.h"

namespace esphome::bthome::testing {
using namespace esphome::bthome::client;

static ble_device_base::ESPBTDevice make_device(const MacAddress &address, int rssi = -50) {
  const uint8_t *mac_msb = address;
  uint8_t mac_lsb[MAC_ADDRESS_SIZE];
  for (size_t i = 0; i < MAC_ADDRESS_SIZE; i++)
    mac_lsb[i] = mac_msb[MAC_ADDRESS_SIZE - i - 1];

  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac_lsb, rssi, ble_device_base::BLE_ADDR_TYPE_PUBLIC, nullptr, 0);
  return device;
}

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
  RemoteDevice<2> device_;
  MockBTHomeRemoteObject handler1_{BTHomeObjectType::BATTERY_PCT};
  MockBTHomeRemoteObject handler2_{BTHomeObjectType::TEMPERATURE_C_E2};

  // Test MAC address: 01:02:03:04:05:06
  MacAddress test_mac_{0x010203040506ULL};
  // Different MAC address: AA:BB:CC:DD:EE:FF
  MacAddress other_mac_{0xAABBCCDDEEFF};

  void SetUp() override {
    // Set the device address
    device_.set_address(test_mac_);
    // Register handlers
    device_.set_handler(0, &handler1_);
    device_.set_handler(1, &handler2_);
  }
};

TEST_F(BTHomeDeviceTest, ParseDataWithMatchingMacAddress) {
  // Create unencrypted BTHome payload with BATTERY_PCT (97%)
  // Header (unencrypted, version 2): 0x40
  // BATTERY_PCT + data: 0x01 0x61
  uint8_t payload[] = {0x40, 0x01, 0x61};

  bool result = device_.parse_data(make_device(test_mac_), payload, sizeof(payload));

  EXPECT_TRUE(result);
  EXPECT_EQ(handler1_.processed_objects().size(), 1);
  EXPECT_EQ(handler2_.processed_objects().size(), 0);
  EXPECT_NEAR(handler1_.processed_objects()[0].value, 97.0f, 0.001f);
}

TEST_F(BTHomeDeviceTest, ParseDataWithNonMatchingMacAddress) {
  // Create unencrypted BTHome payload
  uint8_t payload[] = {0x40, 0x01, 0x61};

  bool result = device_.parse_data(make_device(other_mac_), payload, sizeof(payload));

  // Should return false when MAC address doesn't match
  EXPECT_FALSE(result);
  // Handlers should not be called
  EXPECT_EQ(handler1_.processed_objects().size(), 0);
  EXPECT_EQ(handler2_.processed_objects().size(), 0);
}

TEST_F(BTHomeDeviceTest, ParseEmptyData) {
  EXPECT_TRUE(device_.parse_data(make_device(test_mac_), nullptr, 0));
  EXPECT_EQ(handler1_.processed_objects().size(), 0);
  EXPECT_EQ(handler2_.processed_objects().size(), 0);
}

#ifdef USE_BTHOME_DECRYPTION
TEST_F(BTHomeDeviceTest, ParseKnownEncryptedAdvertisement) {
  RemoteDevice<2> encrypted_device;
  MockBTHomeRemoteObject temperature{BTHomeObjectType::TEMPERATURE_C_E2};
  MockBTHomeRemoteObject humidity{BTHomeObjectType::HUMIDITY_PCT_E2};
  encrypted_device.set_address(MacAddress{0x5448E68F80A5ULL});
  encrypted_device.set_handler(0, &temperature);
  encrypted_device.set_handler(1, &humidity);
  encrypted_device.set_encryption_key(
      {0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1, 0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32});

  // Generated independently with Python cryptography AESCCM. Plaintext is
  // TEMPERATURE_C_E2=25.06 and HUMIDITY_PCT_E2=50.55, counter=12345.
  const uint8_t payload[] = {0x41, 0xA8, 0xF1, 0x60, 0x79, 0xE0, 0x60, 0x39, 0x30, 0x00, 0x00, 0x60, 0x22, 0x10, 0xEE};

  EXPECT_TRUE(encrypted_device.parse_data(make_device(MacAddress{0x5448E68F80A5ULL}), payload, sizeof(payload)));
  ASSERT_EQ(temperature.processed_objects().size(), 1);
  ASSERT_EQ(humidity.processed_objects().size(), 1);
  EXPECT_NEAR(temperature.processed_objects()[0].value, 25.06f, 0.001f);
  EXPECT_NEAR(humidity.processed_objects()[0].value, 50.55f, 0.001f);
}

TEST_F(BTHomeDeviceTest, RejectsTamperedEncryptedAdvertisement) {
  RemoteDevice<1> encrypted_device;
  MockBTHomeRemoteObject temperature{BTHomeObjectType::TEMPERATURE_C_E2};
  encrypted_device.set_address(MacAddress{0x5448E68F80A5ULL});
  encrypted_device.set_handler(0, &temperature);
  encrypted_device.set_encryption_key(
      {0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1, 0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32});
  uint8_t payload[] = {0x41, 0xA8, 0xF1, 0x60, 0x79, 0xE0, 0x60, 0x39, 0x30, 0x00, 0x00, 0x60, 0x22, 0x10, 0xEE};
  payload[1] ^= 0x01;

  EXPECT_TRUE(encrypted_device.parse_data(make_device(MacAddress{0x5448E68F80A5ULL}), payload, sizeof(payload)));
  EXPECT_EQ(temperature.processed_objects().size(), 0);
}
#endif

TEST_F(BTHomeDeviceTest, ParseDataMultipleObjects) {
  // Create unencrypted BTHome payload with BATTERY_PCT and TEMPERATURE_C_E2
  // Header (version 2): 0x40
  // BATTERY_PCT (97%): 0x01 0x61
  // TEMPERATURE_C_E2 (25.06°C): 0x02 0xCA 0x09
  uint8_t payload[] = {0x40, 0x01, 0x61, 0x02, 0xCA, 0x09};

  bool result = device_.parse_data(make_device(test_mac_), payload, sizeof(payload));

  EXPECT_TRUE(result);
  EXPECT_EQ(handler1_.processed_objects().size(), 1);
  EXPECT_EQ(handler2_.processed_objects().size(), 1);
  EXPECT_NEAR(handler1_.processed_objects()[0].value, 97.0f, 0.001f);
  EXPECT_NEAR(handler2_.processed_objects()[0].value, 25.06f, 0.001f);
}

TEST_F(BTHomeDeviceTest, ParseDataWithPacketId) {
  // Create payload with PACKET_ID (0x00) followed by BATTERY_PCT
  // Header (version 2): 0x40
  // PACKET_ID: 0x00 0x42 (packet ID = 66)
  // BATTERY_PCT: 0x01 0x61
  uint8_t payload[] = {0x40, 0x00, 0x42, 0x01, 0x61};

  bool result = device_.parse_data(make_device(test_mac_), payload, sizeof(payload));

  EXPECT_TRUE(result);
  EXPECT_EQ(handler1_.processed_objects().size(), 1);
}

TEST_F(BTHomeDeviceTest, ParseDataDuplicatePacketId) {
  // Create payload with PACKET_ID 0x42
  uint8_t payload[] = {0x40, 0x00, 0x42, 0x01, 0x61};

  // First parse should succeed
  bool result1 = device_.parse_data(make_device(test_mac_), payload, sizeof(payload));
  EXPECT_TRUE(result1);
  EXPECT_EQ(handler1_.processed_objects().size(), 1);

  // Second parse with same packet ID should be ignored
  bool result2 = device_.parse_data(make_device(test_mac_), payload, sizeof(payload));
  EXPECT_TRUE(result2);
  // Handler should not have been called again
  EXPECT_EQ(handler1_.processed_objects().size(), 1);
}

TEST_F(BTHomeDeviceTest, SignalStrengthIgnoresDuplicatePacket) {
  RemoteDevice<1> device;
  MockBTHomeRemoteObject signal_strength{BTHomeObjectType::SIGNAL_STRENGTH};
  device.set_address(test_mac_);
  device.set_handler(0, &signal_strength);
  uint8_t payload[] = {0x40, 0x00, 0x42};

  EXPECT_TRUE(device.parse_data(make_device(test_mac_, -50), payload, sizeof(payload)));
  EXPECT_TRUE(device.parse_data(make_device(test_mac_, -67), payload, sizeof(payload)));

  ASSERT_EQ(signal_strength.processed_objects().size(), 1);
  EXPECT_EQ(signal_strength.processed_objects()[0].value, -50.0f);
}

TEST_F(BTHomeDeviceTest, ParseDataDifferentPacketId) {
  // First payload with PACKET_ID 0x42
  uint8_t payload1[] = {0x40, 0x00, 0x42, 0x01, 0x61};
  bool result1 = device_.parse_data(make_device(test_mac_), payload1, sizeof(payload1));
  EXPECT_TRUE(result1);
  EXPECT_EQ(handler1_.processed_objects().size(), 1);

  // Second payload with PACKET_ID 0x43 (different ID)
  uint8_t payload2[] = {0x40, 0x00, 0x43, 0x01, 0x32};
  bool result2 = device_.parse_data(make_device(test_mac_), payload2, sizeof(payload2));
  EXPECT_TRUE(result2);
  // Handler should be called again because packet ID is different
  EXPECT_EQ(handler1_.processed_objects().size(), 2);
  EXPECT_NEAR(handler1_.processed_objects()[1].value, 50.0f, 0.001f);
}

TEST_F(BTHomeDeviceTest, ParseDataNoMatchingHandler) {
  // Create payload with an object type that doesn't match any handler
  // HUMIDITY_PCT_E2 (0x03) - neither handler expects this
  // Header (version 2): 0x40
  // HUMIDITY_PCT_E2: 0x03 0xBF 0x13
  uint8_t payload[] = {0x40, 0x03, 0xBF, 0x13};

  bool result = device_.parse_data(make_device(test_mac_), payload, sizeof(payload));

  EXPECT_TRUE(result);
  // No handler should have processed the object
  EXPECT_EQ(handler1_.processed_objects().size(), 0);
  EXPECT_EQ(handler2_.processed_objects().size(), 0);
}

TEST_F(BTHomeDeviceTest, ParseDataOnlyUnmatchedObjectsAfterMatch) {
  // Create payload with BATTERY_PCT followed by unmatched object type
  // Header (version 2): 0x40
  // BATTERY_PCT: 0x01 0x61
  // HUMIDITY_PCT_E2: 0x03 0xBF 0x13
  uint8_t payload[] = {0x40, 0x01, 0x61, 0x03, 0xBF, 0x13};

  bool result = device_.parse_data(make_device(test_mac_), payload, sizeof(payload));

  EXPECT_TRUE(result);
  EXPECT_EQ(handler1_.processed_objects().size(), 1);
  EXPECT_EQ(handler2_.processed_objects().size(), 0);
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

  bool result = device3.parse_data(make_device(test_mac_), payload, sizeof(payload));

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
