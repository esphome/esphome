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

// The wrap records Bluedroid's own id; the event that takes it carries that id >> 8 and gattc_if apart
TEST(DiscoveryErrors, EventIdsFindTheStackIdTheWrapRecorded) {
  DiscoveryErrors<3> errors;
  errors.record(0x0103, GATT_INVALID_HANDLE);  // tcb 1, gatt_if 3
  errors.record(0x0003, GATT_ERROR);           // tcb 0, gatt_if 3
  EXPECT_EQ(stack_conn_id(0x01, 3), 0x0103);
  EXPECT_EQ(errors.take(stack_conn_id(0x03, 0)), 0);  // the stripped id alone matches nothing
  EXPECT_EQ(errors.take(stack_conn_id(0x01, 3)), GATT_INVALID_HANDLE);
  EXPECT_EQ(errors.take(stack_conn_id(0x00, 3)), GATT_ERROR);
}

}  // namespace esphome::esp32_ble::testing
