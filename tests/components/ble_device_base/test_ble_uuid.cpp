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

// A default-constructed UUID is UNSET, the historical "not configured" sentinel
// (len 0 through the esp32 get_uuid() adapter).
TEST(BleDeviceUuid, DefaultConstructedIsUnset) {
  const ESPBTUUID unset;
  EXPECT_EQ(unset.type(), ESPBTUUID::Type::UNSET);
  EXPECT_TRUE(unset == ESPBTUUID());
  EXPECT_FALSE(unset == ESPBTUUID::from_uint16(0x1234));
  EXPECT_FALSE(unset.contains(0x00, 0x00));
}

// Every factory yields a non-UNSET UUID, even for 0x0000: only default construction and a
// failed text parse are unset, keeping type() != UNSET equivalent to the old len > 0 check.
TEST(BleDeviceUuid, AllFactoriesProduceSetUuids) {
  const uint8_t raw[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                           0x00, 0x10, 0x00, 0x00, 0x34, 0x12, 0x00, 0x00};
  EXPECT_NE(ESPBTUUID::from_uint16(0x0000).type(), ESPBTUUID::Type::UNSET);
  EXPECT_NE(ESPBTUUID::from_uint32(0).type(), ESPBTUUID::Type::UNSET);
  EXPECT_NE(ESPBTUUID::from_raw(raw).type(), ESPBTUUID::Type::UNSET);
  EXPECT_NE(ESPBTUUID::from_raw_reversed(raw).type(), ESPBTUUID::Type::UNSET);
  EXPECT_NE(ESPBTUUID::from_raw("180F", 4).type(), ESPBTUUID::Type::UNSET);
  EXPECT_NE(ESPBTUUID::from_raw("0000180F", 8).type(), ESPBTUUID::Type::UNSET);
  EXPECT_NE(ESPBTUUID::from_raw(reinterpret_cast<const char *>(raw), 16).type(), ESPBTUUID::Type::UNSET);
  EXPECT_NE(ESPBTUUID::from_raw("6E400001-B5A3-F393-E0A9-E50E24DCCA9E").type(), ESPBTUUID::Type::UNSET);
}

// 0x0000 is a valid short UUID on real devices (esphome/aioesphomeapi#1742); an unset
// UUID must never compare equal to it. Unset equals only unset.
TEST(BleDeviceUuid, UnsetIsNotEqualToZeroUuid) {
  EXPECT_FALSE(ESPBTUUID() == ESPBTUUID::from_uint16(0x0000));
  EXPECT_FALSE(ESPBTUUID::from_uint16(0x0000) == ESPBTUUID());
  const uint8_t base[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                            0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  EXPECT_FALSE(ESPBTUUID() == ESPBTUUID::from_raw(base));
  EXPECT_TRUE(ESPBTUUID() == ESPBTUUID());
  EXPECT_FALSE(ESPBTUUID().as_128bit().is_set());  // widening preserves the unset state
  // A configured 0x0000 still matches its own 128-bit base UUID expansion.
  EXPECT_TRUE(ESPBTUUID::from_uint16(0x0000) == ESPBTUUID::from_raw(base));
}

// is_set() is the sentinel check; an unset UUID prints as "None" instead of a
// valid-looking all-zero 128-bit UUID.
TEST(BleDeviceUuid, IsSetAndUnsetToStr) {
  char buf[UUID_STR_LEN];
  EXPECT_FALSE(ESPBTUUID().is_set());
  EXPECT_STREQ(ESPBTUUID().to_str(buf), "None");
  EXPECT_TRUE(ESPBTUUID::from_uint16(0x0000).is_set());
  EXPECT_STREQ(ESPBTUUID::from_uint16(0x0000).to_str(buf), "0x0000");
}

// Text parsing of an invalid length historically produced a len-0 (unset) UUID.
TEST(BleDeviceUuid, InvalidTextFormParsesToUnset) {
  EXPECT_EQ(ESPBTUUID::from_raw("nope", 3).type(), ESPBTUUID::Type::UNSET);
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
