#include <gtest/gtest.h>

#include "esphome/components/esp32_ble/bluedroid_gattc_discovery.h"

namespace esphome::esp32_ble::testing {

namespace {
constexpr uint8_t GATT_INVALID_HANDLE = 0x01;
constexpr uint8_t GATT_ERROR = 0x85;
}  // namespace

TEST(DiscoveryErrors, TakeReturnsTheRecordedStatusOnce) {
  DiscoveryErrors<3> errors;
  errors.record(0x0003, GATT_INVALID_HANDLE);
  EXPECT_EQ(errors.take(0x0003), GATT_INVALID_HANDLE);
  EXPECT_EQ(errors.take(0x0003), 0);
}

TEST(DiscoveryErrors, NothingRecordedTakesZero) {
  DiscoveryErrors<3> errors;
  EXPECT_EQ(errors.take(0x0003), 0);
}

// Connection id 0 is valid, so a free slot must not read as its entry
TEST(DiscoveryErrors, ConnectionZeroIsKeptApartFromFreeSlots) {
  DiscoveryErrors<3> errors;
  EXPECT_EQ(errors.take(0x0000), 0);
  errors.record(0x0000, GATT_ERROR);
  EXPECT_EQ(errors.take(0x0000), GATT_ERROR);
}

TEST(DiscoveryErrors, ConnectionsAreKeptApart) {
  DiscoveryErrors<3> errors;
  errors.record(0x0003, GATT_INVALID_HANDLE);
  errors.record(0x0103, GATT_ERROR);
  EXPECT_EQ(errors.take(0x0203), 0);
  EXPECT_EQ(errors.take(0x0103), GATT_ERROR);
  EXPECT_EQ(errors.take(0x0003), GATT_INVALID_HANDLE);
}

TEST(DiscoveryErrors, ALaterFailureReplacesTheEarlierOne) {
  DiscoveryErrors<1> errors;
  errors.record(0x0003, GATT_INVALID_HANDLE);
  errors.record(0x0003, GATT_ERROR);
  EXPECT_EQ(errors.take(0x0003), GATT_ERROR);
}

// With every slot taken, a failure is dropped rather than overwriting another connection's
TEST(DiscoveryErrors, FullTableKeepsTheRecordedConnections) {
  DiscoveryErrors<1> errors;
  errors.record(0x0003, GATT_INVALID_HANDLE);
  errors.record(0x0103, GATT_ERROR);
  EXPECT_EQ(errors.take(0x0103), 0);
  EXPECT_EQ(errors.take(0x0003), GATT_INVALID_HANDLE);
}

}  // namespace esphome::esp32_ble::testing
