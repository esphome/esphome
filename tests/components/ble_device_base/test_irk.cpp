#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::ble_device_base::testing {

// Reference vector generated with Python `cryptography` AES-128-ECB following
// the RPA resolution procedure (Bluetooth Core, Vol 3 Part H §2.2.2):
// hash = e(IRK, prand), where prand is the top 3 address bytes and the hash
// must equal the low 3 address bytes.
namespace {
const uint8_t IRK[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                         0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
// 4A:2B:7C:FB:7B:21 — prand 4A:2B:7C (two MSBs = 01, an RPA), hash FB:7B:21.
const uint8_t RPA_LSB_FIRST[6] = {0x21, 0x7b, 0xfb, 0x7c, 0x2b, 0x4a};

ESPBTDevice make_device(const uint8_t mac_lsb_first[6]) {
  ESPBTDevice device;
  device.from_scan_result(mac_lsb_first, /*rssi=*/-60, /*addr_type=*/BLE_ADDR_TYPE_RPA_RANDOM, nullptr, 0);
  return device;
}
}  // namespace

TEST(BleIrk, ResolvesMatchingRpa) {
  ESPBTDevice device = make_device(RPA_LSB_FIRST);
  EXPECT_TRUE(device.resolve_irk(IRK));
}

TEST(BleIrk, RejectsWrongIrk) {
  uint8_t wrong_irk[16];
  for (int i = 0; i < 16; i++)
    wrong_irk[i] = IRK[i] ^ 0xff;
  ESPBTDevice device = make_device(RPA_LSB_FIRST);
  EXPECT_FALSE(device.resolve_irk(wrong_irk));
}

TEST(BleIrk, RejectsWrongAddress) {
  uint8_t other_mac[6];
  for (int i = 0; i < 6; i++)
    other_mac[i] = RPA_LSB_FIRST[i];
  other_mac[0] ^= 0x01;  // corrupt one hash byte
  ESPBTDevice device = make_device(other_mac);
  EXPECT_FALSE(device.resolve_irk(IRK));
}

}  // namespace esphome::ble_device_base::testing
