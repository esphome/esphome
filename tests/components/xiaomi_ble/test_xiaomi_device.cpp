#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

namespace esphome::xiaomi_ble::testing {

// Least significant byte first, as from_scan_result() takes them
static constexpr uint8_t MAC[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
static constexpr uint8_t OTHER_MAC[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x77};

// LYWSDCGQ, frame count 5: temperature 22.5 °C, humidity 30.0 %
static const std::vector<uint8_t> PLAIN_FRAME = {0x50, 0x20, 0xAA, 0x01, 0x05, 0x11, 0x22, 0x33, 0x44,
                                                 0x55, 0x66, 0x0D, 0x10, 0x04, 0xE1, 0x00, 0x2C, 0x01};

// LYWSD03MMC, frame count 7, humidity 30.0 %, encrypted with BINDKEY for MAC
static const std::vector<uint8_t> ENCRYPTED_FRAME = {0x58, 0x58, 0x5B, 0x05, 0x07, 0x11, 0x22, 0x33,
                                                     0x44, 0x55, 0x66, 0x19, 0x5F, 0x86, 0x6A, 0x56,
                                                     0x01, 0x02, 0x03, 0xA4, 0x77, 0xE8, 0x30};
static const char *const BINDKEY = "101112131415161718191a1b1c1d1e1f";
static const char *const WRONG_BINDKEY = "000102030405060708090a0b0c0d0e0f";

class RecordingDevice : public XiaomiDevice {
 public:
  std::vector<XiaomiParseResult> results;

 protected:
  void publish_result_(const XiaomiParseResult &result) override { this->results.push_back(result); }
};

class RecordingEncryptedDevice : public XiaomiEncryptedDevice {
 public:
  std::vector<XiaomiParseResult> results;

 protected:
  void publish_result_(const XiaomiParseResult &result) override { this->results.push_back(result); }
};

static ble_device_base::ESPBTDevice device_from(const uint8_t *mac, const std::vector<uint8_t> &frame) {
  std::vector<uint8_t> adv = {static_cast<uint8_t>(frame.size() + 3), 0x16, 0x95, 0xFE};
  adv.insert(adv.end(), frame.begin(), frame.end());
  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac, -60, 0, adv.data(), static_cast<uint16_t>(adv.size()));
  return device;
}

TEST(XiaomiDevice, PublishesPlainFrame) {
  auto device = device_from(MAC, PLAIN_FRAME);
  RecordingDevice sensor;
  sensor.set_address(device.address_uint64());
  EXPECT_TRUE(sensor.parse_device(device));
  ASSERT_EQ(sensor.results.size(), 1u);
  EXPECT_EQ(sensor.results[0].type, XiaomiParseResult::TYPE_LYWSDCGQ);
  EXPECT_FLOAT_EQ(*sensor.results[0].temperature, 22.5f);
  EXPECT_FLOAT_EQ(*sensor.results[0].humidity, 30.0f);
}

TEST(XiaomiDevice, IgnoresOtherAddress) {
  RecordingDevice sensor;
  sensor.set_address(device_from(OTHER_MAC, PLAIN_FRAME).address_uint64());
  EXPECT_FALSE(sensor.parse_device(device_from(MAC, PLAIN_FRAME)));
  EXPECT_TRUE(sensor.results.empty());
}

TEST(XiaomiDevice, DropsRepeatedFrameCount) {
  auto device = device_from(MAC, PLAIN_FRAME);
  RecordingDevice sensor;
  sensor.set_address(device.address_uint64());
  EXPECT_TRUE(sensor.parse_device(device));
  EXPECT_FALSE(sensor.parse_device(device));
  EXPECT_EQ(sensor.results.size(), 1u);
}

TEST(XiaomiDevice, FrameCountIsPerDevice) {
  auto device_a = device_from(MAC, PLAIN_FRAME);
  auto device_b = device_from(OTHER_MAC, PLAIN_FRAME);
  RecordingDevice sensor_a;
  RecordingDevice sensor_b;
  sensor_a.set_address(device_a.address_uint64());
  sensor_b.set_address(device_b.address_uint64());
  EXPECT_TRUE(sensor_a.parse_device(device_a));
  EXPECT_TRUE(sensor_b.parse_device(device_b));
}

TEST(XiaomiDevice, DecryptsWithoutChangingSharedServiceData) {
  auto device = device_from(MAC, ENCRYPTED_FRAME);
  RecordingEncryptedDevice sensor;
  sensor.set_address(device.address_uint64());
  sensor.set_bindkey(BINDKEY);
  EXPECT_TRUE(sensor.parse_device(device));
  ASSERT_EQ(sensor.results.size(), 1u);
  EXPECT_EQ(sensor.results[0].type, XiaomiParseResult::TYPE_LYWSD03MMC);
  EXPECT_FLOAT_EQ(*sensor.results[0].humidity, 30.0f);
  EXPECT_EQ(device.get_service_datas()[0].data, ENCRYPTED_FRAME);
}

TEST(XiaomiDevice, DropsEncryptedFrameWithoutBindkey) {
  auto device = device_from(MAC, ENCRYPTED_FRAME);
  RecordingDevice sensor;
  sensor.set_address(device.address_uint64());
  EXPECT_FALSE(sensor.parse_device(device));
}

TEST(XiaomiDevice, FailedDecryptionDoesNotAdvanceFrameCount) {
  auto device = device_from(MAC, ENCRYPTED_FRAME);
  RecordingEncryptedDevice sensor;
  sensor.set_address(device.address_uint64());
  sensor.set_bindkey(WRONG_BINDKEY);
  EXPECT_FALSE(sensor.parse_device(device));
  sensor.set_bindkey(BINDKEY);
  EXPECT_TRUE(sensor.parse_device(device));
}

}  // namespace esphome::xiaomi_ble::testing
