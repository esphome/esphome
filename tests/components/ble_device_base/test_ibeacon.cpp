#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::ble_device_base::testing {

// from_manufacturer_data() accepts exactly the iBeacon frame: Apple company ID,
// 23 payload bytes, and the 0x02/0x15 sub-type/length prefix. The prefix check
// is stricter than the legacy esp32 parser (which surfaced any 23-byte Apple
// payload as a beacon) — a declared behavior change; these tests pin the
// accept/reject boundary.
namespace {

ServiceData make_apple_payload(uint8_t sub_type, uint8_t length, size_t size = 23) {
  ServiceData data;
  data.uuid = ESPBTUUID::from_uint16(0x004C);  // Apple company ID
  data.data.assign(size, 0);
  if (size >= 2) {
    data.data[0] = sub_type;
    data.data[1] = length;
  }
  // BeaconData layout: sub_type[0], length[1], proximity_uuid[2..17],
  // major[18..19], minor[20..21], signal_power[22] — all wire values big-endian.
  if (size >= 23) {
    data.data[18] = 0x12;  // major 0x1234
    data.data[19] = 0x34;
    data.data[20] = 0x56;  // minor 0x5678
    data.data[21] = 0x78;
    data.data[22] = 0xC5;  // signal power -59 dBm
  }
  return data;
}

}  // namespace

TEST(BleIBeacon, AcceptsWellFormedFrame) {
  auto beacon = ESPBLEiBeacon::from_manufacturer_data(make_apple_payload(0x02, 0x15));
  ASSERT_TRUE(beacon.has_value());
  // Explicit guard: clang-tidy's unchecked-optional-access models neither
  // gtest's ASSERT_TRUE nor value() as a check.
  if (beacon.has_value()) {
    // Pins every scalar accessor's offset and the on-wire big-endian order.
    EXPECT_EQ(beacon->get_major(), 0x1234);
    EXPECT_EQ(beacon->get_minor(), 0x5678);
    EXPECT_EQ(beacon->get_signal_power(), -59);
  }
}

TEST(BleIBeacon, RejectsWrongSubType) {
  // Apple "nearby" and other frames of coincidental length must not parse.
  EXPECT_FALSE(ESPBLEiBeacon::from_manufacturer_data(make_apple_payload(0x10, 0x15)).has_value());
}

TEST(BleIBeacon, RejectsWrongLengthByte) {
  EXPECT_FALSE(ESPBLEiBeacon::from_manufacturer_data(make_apple_payload(0x02, 0x14)).has_value());
}

TEST(BleIBeacon, RejectsWrongPayloadSize) {
  EXPECT_FALSE(ESPBLEiBeacon::from_manufacturer_data(make_apple_payload(0x02, 0x15, 22)).has_value());
  EXPECT_FALSE(ESPBLEiBeacon::from_manufacturer_data(make_apple_payload(0x02, 0x15, 24)).has_value());
}

TEST(BleIBeacon, RejectsNonAppleCompany) {
  auto data = make_apple_payload(0x02, 0x15);
  data.uuid = ESPBTUUID::from_uint16(0x0059);  // Nordic
  EXPECT_FALSE(ESPBLEiBeacon::from_manufacturer_data(data).has_value());
}

TEST(BleIBeacon, PrefixRejectedFlagsOnlyTheSubTypeCase) {
  // The out-param drives the get_ibeacon() diagnostic for frames the legacy
  // parser accepted: exactly the 23-byte Apple payload with a wrong prefix.
  // Wrong size and non-Apple frames were never accepted and must stay silent.
  bool flagged = false;
  EXPECT_FALSE(ESPBLEiBeacon::from_manufacturer_data(make_apple_payload(0x10, 0x15), &flagged).has_value());
  EXPECT_TRUE(flagged);

  flagged = false;
  ESPBLEiBeacon::from_manufacturer_data(make_apple_payload(0x02, 0x15), &flagged);
  EXPECT_FALSE(flagged);

  flagged = false;
  ESPBLEiBeacon::from_manufacturer_data(make_apple_payload(0x10, 0x15, 22), &flagged);
  EXPECT_FALSE(flagged);

  flagged = false;
  auto nordic = make_apple_payload(0x10, 0x15);
  nordic.uuid = ESPBTUUID::from_uint16(0x0059);
  ESPBLEiBeacon::from_manufacturer_data(nordic, &flagged);
  EXPECT_FALSE(flagged);
}

namespace {

// One AD manufacturer-data record: [len][0xFF][company LE][payload...].
void append_mfr_record(std::vector<uint8_t> &adv, uint16_t company, const std::vector<uint8_t> &payload) {
  adv.push_back(static_cast<uint8_t>(1 + 2 + payload.size()));
  adv.push_back(0xFF);
  adv.push_back(static_cast<uint8_t>(company & 0xFF));
  adv.push_back(static_cast<uint8_t>(company >> 8));
  adv.insert(adv.end(), payload.begin(), payload.end());
}

std::vector<uint8_t> beacon_payload(uint8_t sub_type, uint8_t length) {
  std::vector<uint8_t> p(23, 0);
  p[0] = sub_type;
  p[1] = length;
  p[18] = 0x12;
  p[19] = 0x34;
  p[20] = 0x56;
  p[21] = 0x78;
  p[22] = 0xC5;
  return p;
}

ESPBTDevice device_from(const std::vector<uint8_t> &adv) {
  const uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  ESPBTDevice device;
  device.from_scan_result(mac, -59, 0, adv.data(), static_cast<uint16_t>(adv.size()));
  return device;
}

}  // namespace

// get_ibeacon() wraps the parser with first-rejection capture and the log
// gate; pin its short circuits so a regression there needs a code change, not
// a review, to surface.
TEST(BleIBeacon, GetIbeaconReturnsBeaconDespitePrecedingRejectedFrame) {
  std::vector<uint8_t> adv;
  append_mfr_record(adv, 0x004C, beacon_payload(0x10, 0x15));  // rejected prefix
  append_mfr_record(adv, 0x004C, beacon_payload(0x02, 0x15));  // real iBeacon
  auto device = device_from(adv);
  auto beacon = device.get_ibeacon();
  ASSERT_TRUE(beacon.has_value());
  if (beacon.has_value()) {
    EXPECT_EQ(beacon->get_major(), 0x1234);
  }
}

TEST(BleIBeacon, GetIbeaconEmptyWhenOnlyRejectedFrames) {
  std::vector<uint8_t> adv;
  append_mfr_record(adv, 0x004C, beacon_payload(0x10, 0x15));
  auto device = device_from(adv);
  EXPECT_FALSE(device.get_ibeacon().has_value());
}

TEST(BleIBeacon, GetIbeaconEmptyWithoutManufacturerData) {
  std::vector<uint8_t> adv;
  adv.push_back(0x02);  // flags record only
  adv.push_back(0x01);
  adv.push_back(0x06);
  auto device = device_from(adv);
  EXPECT_FALSE(device.get_ibeacon().has_value());
}

}  // namespace esphome::ble_device_base::testing
