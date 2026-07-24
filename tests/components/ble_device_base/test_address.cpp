#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::ble_device_base::testing {

// from_scan_result() ingests BLE controller order (LSB-first); the public
// accessors must expose the historical esp32 semantics: address() in printable
// (MSB-first) order, address_uint64() with byte 0 in the LSB, address_str()
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

  EXPECT_EQ(device.address_str(), "AA:BB:CC:DD:EE:FF");
}

}  // namespace esphome::ble_device_base::testing
