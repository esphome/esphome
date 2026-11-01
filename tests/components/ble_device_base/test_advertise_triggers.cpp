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

// Controller (LSB-first) order of ADDRESS.
constexpr uint8_t MAC[6] = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
constexpr uint64_t ADDRESS = 0x112233445566ULL;
constexpr uint64_t OTHER_ADDRESS = 0xAABBCCDDEEFFULL;
// 128-bit UUID in BLE wire (little-endian) order, as codegen emits it.
constexpr uint8_t UUID128[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

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

}  // namespace

TEST(BleServiceDataAdvertiseTrigger, RegistersWithTheHub) {
  FakeHub hub;
  BLEServiceDataAdvertiseTrigger trigger(&hub);
  ASSERT_EQ(hub.listeners.size(), 1u);
  EXPECT_EQ(hub.listeners[0], &trigger);
}

TEST(BleServiceDataAdvertiseTrigger, Uuid16MatchFiresWithPayload) {
  FakeHub hub;
  BLEServiceDataAdvertiseTrigger trigger(&hub);
  trigger.set_service_uuid16(0x181A);
  Recorder rec(&trigger);

  std::vector<uint8_t> adv;
  append_ad(adv, AD_SERVICE_DATA_16, {0x1A, 0x18, 0xAA, 0xBB});
  EXPECT_TRUE(trigger.parse_device(device_from(adv)));
  ASSERT_EQ(rec.payloads.size(), 1u);
  EXPECT_EQ(rec.payloads[0], (adv_data_t{0xAA, 0xBB}));
}

TEST(BleServiceDataAdvertiseTrigger, OtherUuidDoesNotFire) {
  FakeHub hub;
  BLEServiceDataAdvertiseTrigger trigger(&hub);
  trigger.set_service_uuid16(0x181A);
  Recorder rec(&trigger);

  std::vector<uint8_t> adv;
  append_ad(adv, AD_SERVICE_DATA_16, {0x1B, 0x18, 0xAA});
  EXPECT_FALSE(trigger.parse_device(device_from(adv)));
  EXPECT_TRUE(rec.payloads.empty());
}

TEST(BleServiceDataAdvertiseTrigger, FirstMatchingEntryFiresOnce) {
  FakeHub hub;
  BLEServiceDataAdvertiseTrigger trigger(&hub);
  trigger.set_service_uuid16(0x181A);
  Recorder rec(&trigger);

  std::vector<uint8_t> adv;
  append_ad(adv, AD_SERVICE_DATA_16, {0x0F, 0x18, 0x01});
  append_ad(adv, AD_SERVICE_DATA_16, {0x1A, 0x18, 0x02});
  append_ad(adv, AD_SERVICE_DATA_16, {0x1A, 0x18, 0x03});
  EXPECT_TRUE(trigger.parse_device(device_from(adv)));
  ASSERT_EQ(rec.payloads.size(), 1u);
  EXPECT_EQ(rec.payloads[0], (adv_data_t{0x02}));
}

TEST(BleServiceDataAdvertiseTrigger, Uuid32Match) {
  FakeHub hub;
  BLEServiceDataAdvertiseTrigger trigger(&hub);
  trigger.set_service_uuid32(0xABCD1234);
  Recorder rec(&trigger);

  std::vector<uint8_t> adv;
  append_ad(adv, AD_SERVICE_DATA_32, {0x34, 0x12, 0xCD, 0xAB, 0x07});
  EXPECT_TRUE(trigger.parse_device(device_from(adv)));
  ASSERT_EQ(rec.payloads.size(), 1u);
  EXPECT_EQ(rec.payloads[0], (adv_data_t{0x07}));
}

TEST(BleServiceDataAdvertiseTrigger, Uuid128Match) {
  FakeHub hub;
  BLEServiceDataAdvertiseTrigger trigger(&hub);
  trigger.set_service_uuid128(UUID128);
  Recorder rec(&trigger);

  // UUID 100F0E0D-0C0B-0A09-0807-060504030201 sent over the air, written out independently of UUID128.
  ASSERT_EQ(ESPBTUUID::from_raw(UUID128), ESPBTUUID::from_raw("100F0E0D-0C0B-0A09-0807-060504030201"));
  std::vector<uint8_t> adv;
  append_ad(adv, AD_SERVICE_DATA_128,
            {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x42});
  EXPECT_TRUE(trigger.parse_device(device_from(adv)));
  ASSERT_EQ(rec.payloads.size(), 1u);
  EXPECT_EQ(rec.payloads[0], (adv_data_t{0x42}));

  // The same bytes in printed (big-endian) order are a different UUID.
  std::vector<uint8_t> reversed;
  append_ad(reversed, AD_SERVICE_DATA_128,
            {0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x43});
  EXPECT_FALSE(trigger.parse_device(device_from(reversed)));
  EXPECT_EQ(rec.payloads.size(), 1u);
}

TEST(BleServiceDataAdvertiseTrigger, ManufacturerDataIsIgnored) {
  FakeHub hub;
  BLEServiceDataAdvertiseTrigger trigger(&hub);
  trigger.set_service_uuid16(0x181A);
  Recorder rec(&trigger);

  std::vector<uint8_t> adv;
  append_ad(adv, AD_MANUFACTURER, {0x1A, 0x18, 0xAA});
  EXPECT_FALSE(trigger.parse_device(device_from(adv)));
  EXPECT_TRUE(rec.payloads.empty());
}

TEST(BleServiceDataAdvertiseTrigger, AddressFilter) {
  FakeHub hub;
  BLEServiceDataAdvertiseTrigger match(&hub);
  match.set_service_uuid16(0x181A);
  match.set_address(ADDRESS);
  Recorder match_rec(&match);
  BLEServiceDataAdvertiseTrigger other(&hub);
  other.set_service_uuid16(0x181A);
  other.set_address(OTHER_ADDRESS);
  Recorder other_rec(&other);

  std::vector<uint8_t> adv;
  append_ad(adv, AD_SERVICE_DATA_16, {0x1A, 0x18, 0x01});
  ESPBTDevice device = device_from(adv);
  EXPECT_TRUE(match.parse_device(device));
  EXPECT_FALSE(other.parse_device(device));
  EXPECT_EQ(match_rec.payloads.size(), 1u);
  EXPECT_TRUE(other_rec.payloads.empty());
}

TEST(BleManufacturerDataAdvertiseTrigger, Uuid16MatchFiresWithPayload) {
  FakeHub hub;
  BLEManufacturerDataAdvertiseTrigger trigger(&hub);
  trigger.set_manufacturer_uuid16(0x004C);
  Recorder rec(&trigger);

  std::vector<uint8_t> adv;
  append_ad(adv, AD_MANUFACTURER, {0x4C, 0x00, 0x02, 0x15});
  EXPECT_TRUE(trigger.parse_device(device_from(adv)));
  ASSERT_EQ(rec.payloads.size(), 1u);
  EXPECT_EQ(rec.payloads[0], (adv_data_t{0x02, 0x15}));
}

TEST(BleManufacturerDataAdvertiseTrigger, ServiceDataIsIgnored) {
  FakeHub hub;
  BLEManufacturerDataAdvertiseTrigger trigger(&hub);
  trigger.set_manufacturer_uuid16(0x004C);
  Recorder rec(&trigger);

  std::vector<uint8_t> adv;
  append_ad(adv, AD_SERVICE_DATA_16, {0x4C, 0x00, 0x02});
  EXPECT_FALSE(trigger.parse_device(device_from(adv)));
  EXPECT_TRUE(rec.payloads.empty());
}

TEST(BleManufacturerDataAdvertiseTrigger, OtherCompanyDoesNotFire) {
  FakeHub hub;
  BLEManufacturerDataAdvertiseTrigger trigger(&hub);
  trigger.set_manufacturer_uuid16(0x004C);
  Recorder rec(&trigger);

  std::vector<uint8_t> adv;
  append_ad(adv, AD_MANUFACTURER, {0x59, 0x00, 0x02});
  EXPECT_FALSE(trigger.parse_device(device_from(adv)));
  EXPECT_TRUE(rec.payloads.empty());
}

TEST(BleManufacturerDataAdvertiseTrigger, AddressFilter) {
  FakeHub hub;
  BLEManufacturerDataAdvertiseTrigger match(&hub);
  match.set_manufacturer_uuid16(0x004C);
  match.set_address(ADDRESS);
  Recorder match_rec(&match);
  BLEManufacturerDataAdvertiseTrigger other(&hub);
  other.set_manufacturer_uuid16(0x004C);
  other.set_address(OTHER_ADDRESS);
  Recorder other_rec(&other);

  std::vector<uint8_t> adv;
  append_ad(adv, AD_MANUFACTURER, {0x4C, 0x00, 0x01});
  ESPBTDevice device = device_from(adv);
  EXPECT_TRUE(match.parse_device(device));
  EXPECT_FALSE(other.parse_device(device));
  EXPECT_EQ(match_rec.payloads.size(), 1u);
  EXPECT_TRUE(other_rec.payloads.empty());
}

}  // namespace esphome::ble_device_base::testing
