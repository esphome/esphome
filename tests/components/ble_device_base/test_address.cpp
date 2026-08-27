#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::ble_device_base::testing {

// from_scan_result() ingests BLE controller order (LSB-first); the public
// accessors must expose the historical esp32 semantics: address() in printable
// (MSB-first) order, address_uint64() with byte 0 in the LSB, address_str_to()
// printed MSB-first.
namespace {
// Device AA:BB:CC:DD:EE:FF — controller order delivers FF first.
const uint8_t MAC_LSB_FIRST[6] = {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa};
}  // namespace

TEST(BleDeviceAddress, AccessorsMatchEsp32Semantics) {
  ESPBTDevice device;
  device.from_scan_result(MAC_LSB_FIRST, -50, BLE_ADDR_TYPE_PUBLIC, nullptr, 0);

  const uint8_t *raw = device.address();
  EXPECT_EQ(raw[0], 0xaa);  // MSB first, like ESP-IDF's bda
  EXPECT_EQ(raw[5], 0xff);

  EXPECT_EQ(device.address_uint64(), 0xAABBCCDDEEFFULL);

  char buf[ESPBTDevice::MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  EXPECT_STREQ(device.address_str_to(buf), "AA:BB:CC:DD:EE:FF");

  // The deprecated wrapper must keep returning the same string until its 2027.2.0 removal.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  EXPECT_EQ(device.address_str(), "AA:BB:CC:DD:EE:FF");
#pragma GCC diagnostic pop
}

// mac_lsb_first_to_uint64() packs the controller-order bytes a raw-advertisement
// callback delivers into the printable-order uint64 the native API speaks — the
// value esp32_ble::ble_addr_to_uint64() has always produced for that address.
TEST(BleDeviceAddress, MacLsbFirstToUint64MatchesWireValue) {
  EXPECT_EQ(mac_lsb_first_to_uint64(MAC_LSB_FIRST), 0xAABBCCDDEEFFULL);
}

// The helper and the parsed-device accessor are two routes to the same wire
// value: byte order must agree no matter which path an advertisement takes.
TEST(BleDeviceAddress, MacLsbFirstToUint64AgreesWithParsedDevice) {
  ESPBTDevice device;
  device.from_scan_result(MAC_LSB_FIRST, -50, BLE_ADDR_TYPE_PUBLIC, nullptr, 0);
  EXPECT_EQ(mac_lsb_first_to_uint64(MAC_LSB_FIRST), device.address_uint64());
}

// uint64_to_mac_msb_first() is the inverse: unpacking the wire value yields
// printable (MSB-first) order, and round-tripping through the LSB-first
// packer restores the original value.
TEST(BleDeviceAddress, Uint64ToMacMsbFirstRoundTrip) {
  uint8_t msb_first[6];
  uint64_to_mac_msb_first(0xAABBCCDDEEFFULL, msb_first);
  EXPECT_EQ(msb_first[0], 0xaa);
  EXPECT_EQ(msb_first[5], 0xff);
  uint8_t lsb_first[6];
  for (int i = 0; i < 6; i++)
    lsb_first[i] = msb_first[5 - i];
  EXPECT_EQ(mac_lsb_first_to_uint64(lsb_first), 0xAABBCCDDEEFFULL);
}

}  // namespace esphome::ble_device_base::testing
