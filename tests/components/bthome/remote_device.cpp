#include <gtest/gtest.h>
#include "esphome/components/bthome/remote_device.h"
#include "esphome/core/helpers.h"

namespace esphome::bthome::testing {
using namespace esphome::bthome::client;

static constexpr size_t BITS_PER_BYTE = 8;
static constexpr uint64_t TEST_MAC = 0x010203040506ULL;
static constexpr uint64_t OTHER_MAC = 0xAABBCCDDEEFFULL;
static constexpr uint64_t ENCRYPTED_TEST_MAC = 0x5448E68F80A5ULL;
static constexpr uint8_t HEADER_V2_MAC_INCLUDED =
    (BTHOME_VERSION_2 << BTHomeHeader::VERSION_SHIFT) | BTHomeHeader::MAC_INCLUDED_FLAG;

static ble_device_base::ESPBTDevice make_device(uint64_t address, int rssi = -50) {
  uint8_t mac_lsb[MAC_ADDRESS_SIZE];
  for (size_t i = 0; i < MAC_ADDRESS_SIZE; i++)
    mac_lsb[i] = static_cast<uint8_t>(address >> (i * BITS_PER_BYTE));

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

  void log_config() override {}

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

  void SetUp() override {
    // Set the device address
    device_.set_address(TEST_MAC);
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

  bool result = device_.parse_data(make_device(TEST_MAC), payload, sizeof(payload));

  EXPECT_TRUE(result);
  EXPECT_EQ(handler1_.processed_objects().size(), 1);
  EXPECT_EQ(handler2_.processed_objects().size(), 0);
  EXPECT_NEAR(handler1_.processed_objects()[0].value, 97.0f, 0.001f);
}

TEST_F(BTHomeDeviceTest, ParseDataWithNonMatchingMacAddress) {
  // Create unencrypted BTHome payload
  uint8_t payload[] = {0x40, 0x01, 0x61};

  bool result = device_.parse_data(make_device(OTHER_MAC), payload, sizeof(payload));

  // Should return false when MAC address doesn't match
  EXPECT_FALSE(result);
  // Handlers should not be called
  EXPECT_EQ(handler1_.processed_objects().size(), 0);
  EXPECT_EQ(handler2_.processed_objects().size(), 0);
}

TEST_F(BTHomeDeviceTest, ParseEmptyData) {
  EXPECT_TRUE(device_.parse_data(make_device(TEST_MAC), nullptr, 0));
  EXPECT_EQ(handler1_.processed_objects().size(), 0);
  EXPECT_EQ(handler2_.processed_objects().size(), 0);
}

#ifdef USE_BTHOME_DECRYPTION
TEST_F(BTHomeDeviceTest, ParseKnownEncryptedAdvertisement) {
  RemoteDevice<2> encrypted_device;
  MockBTHomeRemoteObject temperature{BTHomeObjectType::TEMPERATURE_C_E2};
  MockBTHomeRemoteObject humidity{BTHomeObjectType::HUMIDITY_PCT_E2};
  encrypted_device.set_address(ENCRYPTED_TEST_MAC);
  encrypted_device.set_handler(0, &temperature);
  encrypted_device.set_handler(1, &humidity);
  encrypted_device.set_encryption_key(
      {0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1, 0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32});

  // Generated independently with Python cryptography AESCCM. Plaintext is
  // TEMPERATURE_C_E2=25.06 and HUMIDITY_PCT_E2=50.55, counter=12345.
  const uint8_t payload[] = {0x41, 0xA8, 0xF1, 0x60, 0x79, 0xE0, 0x60, 0x39, 0x30, 0x00, 0x00, 0x60, 0x22, 0x10, 0xEE};

  EXPECT_TRUE(encrypted_device.parse_data(make_device(ENCRYPTED_TEST_MAC), payload, sizeof(payload)));
  ASSERT_EQ(temperature.processed_objects().size(), 1);
  ASSERT_EQ(humidity.processed_objects().size(), 1);
  EXPECT_NEAR(temperature.processed_objects()[0].value, 25.06f, 0.001f);
  EXPECT_NEAR(humidity.processed_objects()[0].value, 50.55f, 0.001f);
}

TEST_F(BTHomeDeviceTest, RejectsTamperedEncryptedAdvertisement) {
  RemoteDevice<1> encrypted_device;
  MockBTHomeRemoteObject temperature{BTHomeObjectType::TEMPERATURE_C_E2};
  encrypted_device.set_address(ENCRYPTED_TEST_MAC);
  encrypted_device.set_handler(0, &temperature);
  encrypted_device.set_encryption_key(
      {0x23, 0x1D, 0x39, 0xC1, 0xD7, 0xCC, 0x1A, 0xB1, 0xAE, 0xE2, 0x24, 0xCD, 0x09, 0x6D, 0xB9, 0x32});
  uint8_t payload[] = {0x41, 0xA8, 0xF1, 0x60, 0x79, 0xE0, 0x60, 0x39, 0x30, 0x00, 0x00, 0x60, 0x22, 0x10, 0xEE};
  payload[1] ^= 0x01;

  EXPECT_TRUE(encrypted_device.parse_data(make_device(ENCRYPTED_TEST_MAC), payload, sizeof(payload)));
  EXPECT_EQ(temperature.processed_objects().size(), 0);
}
#endif

TEST_F(BTHomeDeviceTest, ParseDataMultipleObjects) {
  // Create unencrypted BTHome payload with BATTERY_PCT and TEMPERATURE_C_E2
  // Header (version 2): 0x40
  // BATTERY_PCT (97%): 0x01 0x61
  // TEMPERATURE_C_E2 (25.06°C): 0x02 0xCA 0x09
  uint8_t payload[] = {0x40, 0x01, 0x61, 0x02, 0xCA, 0x09};

  bool result = device_.parse_data(make_device(TEST_MAC), payload, sizeof(payload));

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

  bool result = device_.parse_data(make_device(TEST_MAC), payload, sizeof(payload));

  EXPECT_TRUE(result);
  EXPECT_EQ(handler1_.processed_objects().size(), 1);
}

TEST_F(BTHomeDeviceTest, ParseDataDuplicatePacketId) {
  // Create payload with PACKET_ID 0x42
  uint8_t payload[] = {0x40, 0x00, 0x42, 0x01, 0x61};

  // First parse should succeed
  bool result1 = device_.parse_data(make_device(TEST_MAC), payload, sizeof(payload));
  EXPECT_TRUE(result1);
  EXPECT_EQ(handler1_.processed_objects().size(), 1);

  // Second parse with same packet ID should be ignored
  bool result2 = device_.parse_data(make_device(TEST_MAC), payload, sizeof(payload));
  EXPECT_TRUE(result2);
  // Handler should not have been called again
  EXPECT_EQ(handler1_.processed_objects().size(), 1);
}

TEST_F(BTHomeDeviceTest, SignalStrengthIgnoresDuplicatePacket) {
  RemoteDevice<1> device;
  MockBTHomeRemoteObject signal_strength{BTHomeObjectType::SIGNAL_STRENGTH};
  device.set_address(TEST_MAC);
  device.set_handler(0, &signal_strength);
  uint8_t payload[] = {0x40, 0x00, 0x42};

  EXPECT_TRUE(device.parse_data(make_device(TEST_MAC, -50), payload, sizeof(payload)));
  EXPECT_TRUE(device.parse_data(make_device(TEST_MAC, -67), payload, sizeof(payload)));

  ASSERT_EQ(signal_strength.processed_objects().size(), 1);
  EXPECT_EQ(signal_strength.processed_objects()[0].value, -50.0f);
}

TEST_F(BTHomeDeviceTest, ParseDataDifferentPacketId) {
  // First payload with PACKET_ID 0x42
  uint8_t payload1[] = {0x40, 0x00, 0x42, 0x01, 0x61};
  bool result1 = device_.parse_data(make_device(TEST_MAC), payload1, sizeof(payload1));
  EXPECT_TRUE(result1);
  EXPECT_EQ(handler1_.processed_objects().size(), 1);

  // Second payload with PACKET_ID 0x43 (different ID)
  uint8_t payload2[] = {0x40, 0x00, 0x43, 0x01, 0x32};
  bool result2 = device_.parse_data(make_device(TEST_MAC), payload2, sizeof(payload2));
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

  bool result = device_.parse_data(make_device(TEST_MAC), payload, sizeof(payload));

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

  bool result = device_.parse_data(make_device(TEST_MAC), payload, sizeof(payload));

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

  device3.set_address(TEST_MAC);
  device3.set_handler(0, &temp_handler1);
  device3.set_handler(1, &temp_handler2);
  device3.set_handler(2, &batt_handler);

  // Payload with two TEMPERATURE_C_E2 objects and one BATTERY_PCT
  // Header: 0x40
  // TEMPERATURE_C_E2 (25.06°C): 0x02 0xCA 0x09
  // TEMPERATURE_C_E2 (47.23°C): 0x02 0x73 0x12
  // BATTERY_PCT (97%): 0x01 0x61
  uint8_t payload[] = {0x40, 0x02, 0xCA, 0x09, 0x02, 0x73, 0x12, 0x01, 0x61};

  bool result = device3.parse_data(make_device(TEST_MAC), payload, sizeof(payload));

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

TEST_F(BTHomeDeviceTest, EncryptedFrameWithoutKeyIsIgnored) {
  // Encrypted header (version 2, encryption flag) followed by bytes that look like BATTERY_PCT
  static constexpr uint8_t HEADER_V2_ENCRYPTED =
      (BTHOME_VERSION_2 << BTHomeHeader::VERSION_SHIFT) | BTHomeHeader::ENCRYPTED_FLAG;
  uint8_t payload[] = {HEADER_V2_ENCRYPTED, 0x01, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  // Ignored every time; the warning is only logged for the first one.
  EXPECT_TRUE(device_.parse_data(make_device(TEST_MAC), payload, sizeof(payload)));
  EXPECT_TRUE(device_.parse_data(make_device(TEST_MAC), payload, sizeof(payload)));
  EXPECT_EQ(handler1_.processed_objects().size(), 0);
}

TEST_F(BTHomeDeviceTest, EmbeddedMacMatchesWhenBleAddressDiffers) {
  // Embedded MAC 01:02:03:04:05:06 (least significant byte first), then BATTERY_PCT 97%
  uint8_t payload[] = {HEADER_V2_MAC_INCLUDED, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x01, 0x61};

  EXPECT_TRUE(device_.parse_data(make_device(OTHER_MAC), payload, sizeof(payload)));
  ASSERT_EQ(handler1_.processed_objects().size(), 1);
  EXPECT_NEAR(handler1_.processed_objects()[0].value, 97.0f, 0.001f);
}

TEST_F(BTHomeDeviceTest, EmbeddedMacBytesAreNotDecodedAsObjects) {
  RemoteDevice<2> device;
  MockBTHomeRemoteObject battery{BTHomeObjectType::BATTERY_PCT};
  MockBTHomeRemoteObject humidity{BTHomeObjectType::HUMIDITY_PCT_E2};
  device.set_address(TEST_MAC);
  device.set_handler(0, &battery);
  device.set_handler(1, &humidity);
  // Without skipping, the MAC bytes would decode as MASS_KG_E2 (0x06) and HUMIDITY_PCT_E2 (0x03)
  uint8_t payload[] = {HEADER_V2_MAC_INCLUDED, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x01, 0x61};

  EXPECT_TRUE(device.parse_data(make_device(TEST_MAC), payload, sizeof(payload)));
  ASSERT_EQ(battery.processed_objects().size(), 1);
  EXPECT_NEAR(battery.processed_objects()[0].value, 97.0f, 0.001f);
  EXPECT_EQ(humidity.processed_objects().size(), 0);
}

TEST_F(BTHomeDeviceTest, EmbeddedMacMismatchIsIgnored) {
  // Embedded MAC AA:BB:CC:DD:EE:FF does not match either
  uint8_t payload[] = {HEADER_V2_MAC_INCLUDED, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x01, 0x61};

  EXPECT_FALSE(device_.parse_data(make_device(OTHER_MAC), payload, sizeof(payload)));
  EXPECT_EQ(handler1_.processed_objects().size(), 0);
}

TEST_F(BTHomeDeviceTest, PayloadTooShortForEmbeddedMacIsDropped) {
  uint8_t payload[] = {HEADER_V2_MAC_INCLUDED, 0x06, 0x05, 0x04};

  EXPECT_TRUE(device_.parse_data(make_device(TEST_MAC), payload, sizeof(payload)));
  EXPECT_EQ(handler1_.processed_objects().size(), 0);
  EXPECT_EQ(handler2_.processed_objects().size(), 0);
}

// Captured from pvvx ATC_MiThermometer firmware, which alternates two payloads under one packet ID.
// A: PACKET_ID 0xB3, VOLTAGE_V_E3 2.749 V, POWER_ON off, OPENING_OPEN open
static const uint8_t PVVX_PAYLOAD_A[] = {0x40, 0x00, 0xB3, 0x0C, 0xBD, 0x0A, 0x10, 0x00, 0x11, 0x01};
// B: PACKET_ID 0xB3, BATTERY_PCT 68%, 2x TEMPERATURE_C_E2, HUMIDITY_PCT_E2, RUNNING_ACTIVE off
static const uint8_t PVVX_PAYLOAD_B[] = {0x40, 0x00, 0xB3, 0x01, 0x44, 0x02, 0x8B, 0x08,
                                         0x02, 0x34, 0x08, 0x03, 0x34, 0x11, 0x27, 0x00};

class BTHomeAlternatingPayloadTest : public ::testing::Test {
 protected:
  RemoteDevice<2> device_;
  MockBTHomeRemoteObject battery_{BTHomeObjectType::BATTERY_PCT};
  MockBTHomeRemoteObject voltage_{BTHomeObjectType::VOLTAGE_V_E3};

  void SetUp() override {
    this->device_.set_address(TEST_MAC);
    this->device_.set_handler(0, &this->battery_);
    this->device_.set_handler(1, &this->voltage_);
  }

  bool parse_(const uint8_t *payload, size_t size) {
    return this->device_.parse_data(make_device(TEST_MAC), payload, size);
  }
};

TEST_F(BTHomeAlternatingPayloadTest, DifferentPayloadsWithSamePacketIdAreProcessed) {
  EXPECT_TRUE(this->parse_(PVVX_PAYLOAD_A, sizeof(PVVX_PAYLOAD_A)));
  EXPECT_TRUE(this->parse_(PVVX_PAYLOAD_B, sizeof(PVVX_PAYLOAD_B)));

  ASSERT_EQ(this->voltage_.processed_objects().size(), 1);
  EXPECT_NEAR(this->voltage_.processed_objects()[0].value, 2.749f, 0.001f);
  ASSERT_EQ(this->battery_.processed_objects().size(), 1);
  EXPECT_NEAR(this->battery_.processed_objects()[0].value, 68.0f, 0.001f);
}

TEST_F(BTHomeAlternatingPayloadTest, RepeatedPayloadsWithSamePacketIdAreIgnored) {
  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(this->parse_(PVVX_PAYLOAD_A, sizeof(PVVX_PAYLOAD_A)));
    EXPECT_TRUE(this->parse_(PVVX_PAYLOAD_B, sizeof(PVVX_PAYLOAD_B)));
  }

  EXPECT_EQ(this->voltage_.processed_objects().size(), 1);
  EXPECT_EQ(this->battery_.processed_objects().size(), 1);
}

TEST_F(BTHomeAlternatingPayloadTest, SamePayloadWithNewPacketIdIsProcessed) {
  static constexpr size_t PACKET_ID_VALUE_OFFSET = sizeof(BTHomeHeader) + 1;
  uint8_t next_packet[sizeof(PVVX_PAYLOAD_A)];
  std::copy(std::begin(PVVX_PAYLOAD_A), std::end(PVVX_PAYLOAD_A), next_packet);
  next_packet[PACKET_ID_VALUE_OFFSET]++;

  EXPECT_TRUE(this->parse_(PVVX_PAYLOAD_A, sizeof(PVVX_PAYLOAD_A)));
  EXPECT_TRUE(this->parse_(next_packet, sizeof(next_packet)));

  EXPECT_EQ(this->voltage_.processed_objects().size(), 2);
}

}  // namespace esphome::bthome::testing
