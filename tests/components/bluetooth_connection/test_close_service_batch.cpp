#include "esphome/components/bluetooth_connection/bluetooth_connection.h"

#include <gtest/gtest.h>

#include "esphome/components/api/api_pb2.h"

namespace esphome::bluetooth_connection {

// The three cursor behaviors: a fitting service advances and continues, an
// overflowing batch with >1 service pops and retries it, and a single
// oversized service is force-advanced so the stream cannot wedge.

static void add_service(api::BluetoothGATTGetServicesResponse &resp, uint16_t characteristics) {
  resp.services.emplace_back();
  auto &svc = resp.services.back();
  svc.handle = resp.services.size();
  svc.uuid = {0x1234567890ABCDEFULL, 0xFEDCBA0987654321ULL};
  svc.characteristics.init(characteristics);
  for (uint16_t i = 0; i < characteristics; i++) {
    auto &chr = svc.characteristics.emplace_back();
    chr.handle = 100 + i;
    chr.properties = 0x12;
    chr.uuid = {0x1234567890ABCDEFULL, 0xFEDCBA0987654321ULL};
  }
}

TEST(CloseServiceBatch, FittingServiceAdvancesAndContinues) {
  api::BluetoothGATTGetServicesResponse resp;
  add_service(resp, 1);
  size_t current_size = 0;
  int16_t cursor = 0;
  EXPECT_EQ(close_service_batch(resp, current_size, cursor, 0, "AA:BB"), BatchClose::CONTINUE);
  EXPECT_EQ(cursor, 1);
  EXPECT_GT(current_size, 0u);
}

TEST(CloseServiceBatch, OverflowPopsAndRetriesWithoutAdvancing) {
  api::BluetoothGATTGetServicesResponse resp;
  add_service(resp, 1);
  add_service(resp, 1);
  size_t current_size = MAX_PACKET_SIZE - 10;  // any service is bigger than 10 bytes
  int16_t cursor = 5;
  EXPECT_EQ(close_service_batch(resp, current_size, cursor, 0, "AA:BB"), BatchClose::SEND);
  EXPECT_EQ(resp.services.size(), 1u);  // popped for the next batch
  EXPECT_EQ(cursor, 5);                 // not advanced: retried next batch
}

TEST(CloseServiceBatch, SingleOversizedServiceForceAdvances) {
  api::BluetoothGATTGetServicesResponse resp;
  add_service(resp, 60);  // ~30 bytes per characteristic, far past the budget
  ASSERT_GT(resp.services.back().calculate_size(), MAX_PACKET_SIZE);
  size_t current_size = 0;
  int16_t cursor = 7;
  EXPECT_EQ(close_service_batch(resp, current_size, cursor, 0, "AA:BB"), BatchClose::SEND);
  EXPECT_EQ(cursor, 8);  // advanced despite not fitting, so the stream moves on
}

}  // namespace esphome::bluetooth_connection
