// Pins the shared UUID wire packing and the size-estimate budget the service
// streamers rely on, in both efficient and legacy client modes.
#include "esphome/components/bluetooth_connection/bluetooth_connection.h"

#include <gtest/gtest.h>

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

}  // namespace esphome::bluetooth_connection::testing
