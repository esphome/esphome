#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::ble_device_base::testing {

// A 16- or 32-bit UUID must compare equal to its 128-bit Bluetooth Base UUID form, matching
// esp32_ble_tracker. The 128-bit raw is the base UUID (LSB-first) with the short value at
// bytes 12.. : here 0x1234 -> bytes [12]=0x34, [13]=0x12.
TEST(BleDeviceUuid, ShortFormMatchesEquivalentLongForm) {
  const ESPBTUUID u16 = ESPBTUUID::from_uint16(0x1234);
  const uint8_t raw128[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                              0x00, 0x10, 0x00, 0x00, 0x34, 0x12, 0x00, 0x00};
  const ESPBTUUID u128 = ESPBTUUID::from_raw(raw128);
  EXPECT_TRUE(u16 == u128);
  EXPECT_TRUE(u128 == u16);  // symmetric
}

TEST(BleDeviceUuid, ThirtyTwoBitMatchesEquivalentLongForm) {
  const ESPBTUUID u32 = ESPBTUUID::from_uint32(0x1122AAFF);
  const uint8_t raw128[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                              0x00, 0x10, 0x00, 0x00, 0xFF, 0xAA, 0x22, 0x11};
  const ESPBTUUID u128 = ESPBTUUID::from_raw(raw128);
  EXPECT_TRUE(u32 == u128);
}

TEST(BleDeviceUuid, DifferentUuidsDoNotMatch) {
  EXPECT_FALSE(ESPBTUUID::from_uint16(0x1234) == ESPBTUUID::from_uint16(0x1235));
  const uint8_t raw128[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                              0x00, 0x10, 0x00, 0x00, 0x34, 0x12, 0x00, 0x00};
  // Same low bytes but a non-base prefix is a genuinely different 128-bit UUID.
  uint8_t custom[16];
  memcpy(custom, raw128, 16);
  custom[0] ^= 0x01;
  EXPECT_FALSE(ESPBTUUID::from_uint16(0x1234) == ESPBTUUID::from_raw(custom));
}

}  // namespace esphome::ble_device_base::testing
