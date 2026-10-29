#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/ble_device_base/automation.h"
#include "esphome/core/base_automation.h"

namespace esphome::ble_device_base::testing {
namespace {

constexpr uint8_t AD_MANUFACTURER = 0xFF;
constexpr uint8_t AD_SERVICE_DATA_16 = 0x16;
constexpr uint8_t AD_SERVICE_DATA_32 = 0x20;
constexpr uint8_t AD_SERVICE_DATA_128 = 0x21;

// Controller (LSB-first) order of 11:22:33:44:55:66.
constexpr uint8_t MAC[6] = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
// 128-bit UUID in BLE wire (little-endian) order, as codegen emits it.
constexpr uint8_t UUID128[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

constexpr ESPBTUUID UUID_181A = ESPBTUUID::from_uint16(0x181A);
constexpr ESPBTUUID UUID_ABCD1234 = ESPBTUUID::from_uint32(0xABCD1234);
constexpr ESPBTUUID UUID_128 = ESPBTUUID::from_raw(UUID128);

struct FakeHub {
  void register_listener(ESPBTDeviceListener *listener) { this->listeners.push_back(listener); }
  std::vector<ESPBTDeviceListener *> listeners;
};

// Records the payload of every firing of an adv_data_t trigger.
struct Recorder {
  explicit Recorder(Trigger<const adv_data_t &> *trigger) : automation(trigger) {
    this->automation.add_action(&this->action);
  }
  std::vector<adv_data_t> payloads;
  LambdaAction<const adv_data_t &> action{[this](const adv_data_t &x) { this->payloads.push_back(x); }};
  Automation<const adv_data_t &> automation;
};

void append_ad(std::vector<uint8_t> &adv, uint8_t ad_type, const std::vector<uint8_t> &body) {
  adv.push_back(static_cast<uint8_t>(body.size() + 1));
  adv.push_back(ad_type);
  adv.insert(adv.end(), body.begin(), body.end());
}

ESPBTDevice device_from(const std::vector<uint8_t> &adv) {
  ESPBTDevice device;
  device.from_scan_result(MAC, -59, 0, adv.data(), static_cast<uint16_t>(adv.size()));
  return device;
}

const adv_data_t *service_181a(const ESPBTDevice &device) {
  return find_adv_data(device.get_service_datas(), UUID_181A);
}

}  // namespace

TEST(BLEAdvertiseDataTrigger, RegistersWithTheHub) {
  FakeHub hub;
  BLEAdvertiseDataTrigger trigger(&hub, service_181a);
  ASSERT_EQ(hub.listeners.size(), 1u);
  EXPECT_EQ(hub.listeners[0], &trigger);
}

TEST(BLEAdvertiseDataTrigger, MatchFiresOnceWithPayload) {
  FakeHub hub;
  BLEAdvertiseDataTrigger trigger(&hub, service_181a);
  Recorder rec(&trigger);

  std::vector<uint8_t> adv;
  append_ad(adv, AD_SERVICE_DATA_16, {0x1A, 0x18, 0xAA, 0xBB});
  EXPECT_TRUE(trigger.parse_device(device_from(adv)));
  ASSERT_EQ(rec.payloads.size(), 1u);
  EXPECT_EQ(rec.payloads[0], (adv_data_t{0xAA, 0xBB}));
}

TEST(BLEAdvertiseDataTrigger, NoMatchDoesNotFire) {
  FakeHub hub;
  BLEAdvertiseDataTrigger trigger(&hub, service_181a);
  Recorder rec(&trigger);

  std::vector<uint8_t> adv;
  append_ad(adv, AD_SERVICE_DATA_16, {0x1B, 0x18, 0xAA});
  EXPECT_FALSE(trigger.parse_device(device_from(adv)));
  EXPECT_TRUE(rec.payloads.empty());
}

TEST(FindAdvData, ReturnsTheFirstEntryWithTheUuid) {
  std::vector<uint8_t> adv;
  append_ad(adv, AD_SERVICE_DATA_16, {0x0F, 0x18, 0x01});
  append_ad(adv, AD_SERVICE_DATA_16, {0x1A, 0x18, 0x02});
  append_ad(adv, AD_SERVICE_DATA_16, {0x1A, 0x18, 0x03});
  ESPBTDevice device = device_from(adv);
  const adv_data_t *data = find_adv_data(device.get_service_datas(), UUID_181A);
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(*data, (adv_data_t{0x02}));
}

TEST(FindAdvData, NoEntryWithTheUuid) {
  std::vector<uint8_t> adv;
  append_ad(adv, AD_SERVICE_DATA_16, {0x0F, 0x18, 0x01});
  append_ad(adv, AD_MANUFACTURER, {0x1A, 0x18, 0x02});
  ESPBTDevice device = device_from(adv);
  EXPECT_EQ(find_adv_data(device.get_service_datas(), UUID_181A), nullptr);
  EXPECT_EQ(find_adv_data({}, UUID_181A), nullptr);
}

TEST(FindAdvData, MatchesEveryUuidWidth) {
  std::vector<uint8_t> adv;
  append_ad(adv, AD_SERVICE_DATA_32, {0x34, 0x12, 0xCD, 0xAB, 0x07});
  std::vector<uint8_t> body(UUID128, UUID128 + 16);
  body.push_back(0x42);
  append_ad(adv, AD_SERVICE_DATA_128, body);
  append_ad(adv, AD_MANUFACTURER, {0x1A, 0x18, 0x09});
  ESPBTDevice device = device_from(adv);

  const adv_data_t *data32 = find_adv_data(device.get_service_datas(), UUID_ABCD1234);
  ASSERT_NE(data32, nullptr);
  EXPECT_EQ(*data32, (adv_data_t{0x07}));
  const adv_data_t *data128 = find_adv_data(device.get_service_datas(), UUID_128);
  ASSERT_NE(data128, nullptr);
  EXPECT_EQ(*data128, (adv_data_t{0x42}));
  const adv_data_t *manufacturer = find_adv_data(device.get_manufacturer_datas(), UUID_181A);
  ASSERT_NE(manufacturer, nullptr);
  EXPECT_EQ(*manufacturer, (adv_data_t{0x09}));
}

TEST(ESPBTUUID, ConstexprFactoriesMatchRuntimeOnes) {
  const uint8_t *volatile raw = UUID128;  // forces the runtime path
  EXPECT_EQ(UUID_181A, ESPBTUUID::from_raw("181A"));
  EXPECT_EQ(UUID_ABCD1234.type(), ESPBTUUID::Type::UUID32);
  EXPECT_EQ(UUID_ABCD1234.uuid32(), 0xABCD1234u);
  EXPECT_EQ(UUID_128, ESPBTUUID::from_raw(raw));
  EXPECT_EQ(UUID_128.type(), ESPBTUUID::Type::UUID128);
}

}  // namespace esphome::ble_device_base::testing
