// Pins the shared UUID wire packing and the size-estimate budget the service
// streamers rely on, in both efficient and legacy client modes.
#include "esphome/components/bluetooth_connection/bluetooth_connection.h"

#include <gtest/gtest.h>

#include <array>
#include <cstring>

namespace esphome::bluetooth_connection::testing {

using ble_device_base::ESPBTUUID;

TEST(GattUuidPacking, ShortUuidUsedWhenClientSupportsIt) {
  std::array<uint64_t, 2> uuid128{};
  uint32_t short_uuid = 0;
  fill_gatt_uuid(uuid128, short_uuid, ESPBTUUID::from_uint16(0x180F), true);
  EXPECT_EQ(short_uuid, 0x180Fu);
  EXPECT_EQ(uuid128[0], 0u);
  EXPECT_EQ(uuid128[1], 0u);
}

TEST(GattUuidPacking, LegacyClientGetsBaseUuidExpansion) {
  // 0000180F-0000-1000-8000-00805F9B34FB
  std::array<uint64_t, 2> uuid128{};
  uint32_t short_uuid = 0;
  fill_gatt_uuid(uuid128, short_uuid, ESPBTUUID::from_uint16(0x180F), false);
  EXPECT_EQ(short_uuid, 0u);
  EXPECT_EQ(uuid128[0], 0x0000180F00001000ULL);
  EXPECT_EQ(uuid128[1], 0x800000805F9B34FBULL);
}

TEST(GattUuidPacking, FullUuidPassesThroughBigEndian) {
  // 12345678-90AB-CDEF-1122-334455667788, stored little-endian in ESPBTUUID.
  const uint8_t big_endian[16] = {0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF,
                                  0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  std::array<uint64_t, 2> uuid128{};
  uint32_t short_uuid = 0;
  // Efficient mode must still use the 128-bit form for 128-bit UUIDs.
  fill_gatt_uuid(uuid128, short_uuid, ESPBTUUID::from_raw_reversed(big_endian), true);
  EXPECT_EQ(short_uuid, 0u);
  EXPECT_EQ(uuid128[0], 0x1234567890ABCDEFULL);
  EXPECT_EQ(uuid128[1], 0x1122334455667788ULL);
}

TEST(GattUuidPacking, EstimateGrowsWithCharacteristicsAndMode) {
  // The estimate only gates batching; pin its shape, not exact bytes.
  EXPECT_LT(estimate_service_size(0, true), estimate_service_size(0, false));
  EXPECT_LT(estimate_service_size(1, false), estimate_service_size(2, false));
}

// BTstack hands every GATT UUID over as 128 bits, big-endian; uuid_from_be128 picks the short form
static constexpr uint8_t BASE_TAIL[12] = {0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};

static ESPBTUUID on_base(uint32_t value) {
  uint8_t be[16] = {static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
                    static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
  memcpy(be + 4, BASE_TAIL, sizeof(BASE_TAIL));
  return uuid_from_be128(be);
}

TEST(GattUuidFromBe128, SixteenBitOnBase) {
  const ESPBTUUID uuid = on_base(0x180F);
  ASSERT_EQ(uuid.type(), ESPBTUUID::Type::UUID16);
  EXPECT_EQ(uuid.uuid16(), 0x180F);
}

TEST(GattUuidFromBe128, ThirtyTwoBitOnBaseKeepsTopHalf) {
  // 5EC10001-0000-1000-8000-00805F9B34FB: BTstack's own uuid16 is 0x0001 for this one
  const ESPBTUUID uuid = on_base(0x5EC10001);
  ASSERT_EQ(uuid.type(), ESPBTUUID::Type::UUID32);
  EXPECT_EQ(uuid.uuid32(), 0x5EC10001u);

  std::array<uint64_t, 2> uuid128{};
  uint32_t short_uuid = 0;
  fill_gatt_uuid(uuid128, short_uuid, uuid, true);
  EXPECT_EQ(short_uuid, 0x5EC10001u);

  short_uuid = 0;
  fill_gatt_uuid(uuid128, short_uuid, uuid, false);
  EXPECT_EQ(short_uuid, 0u);
  EXPECT_EQ(uuid128[0], 0x5EC1000100001000ULL);
  EXPECT_EQ(uuid128[1], 0x800000805F9B34FBULL);
}

TEST(GattUuidFromBe128, ShortFormBoundary) {
  EXPECT_EQ(on_base(0xFFFF).type(), ESPBTUUID::Type::UUID16);
  const ESPBTUUID uuid = on_base(0x10000);
  ASSERT_EQ(uuid.type(), ESPBTUUID::Type::UUID32);
  EXPECT_EQ(uuid.uuid32(), 0x10000u);
}

TEST(GattUuidFromBe128, OffBaseStaysFullWidth) {
  const uint8_t big_endian[16] = {0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF,
                                  0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  const ESPBTUUID uuid = uuid_from_be128(big_endian);
  ASSERT_EQ(uuid.type(), ESPBTUUID::Type::UUID128);
  EXPECT_EQ(uuid, ESPBTUUID::from_raw_reversed(big_endian));

  // One byte off the base tail is a different 128-bit UUID, not a short one
  uint8_t near_base[16] = {0x00, 0x00, 0x18, 0x0F};
  memcpy(near_base + 4, BASE_TAIL, sizeof(BASE_TAIL));
  near_base[15] ^= 1;
  EXPECT_EQ(uuid_from_be128(near_base).type(), ESPBTUUID::Type::UUID128);
}

}  // namespace esphome::bluetooth_connection::testing
