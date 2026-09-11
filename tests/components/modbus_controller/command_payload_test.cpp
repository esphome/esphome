#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/modbus_controller/modbus_controller.h"

// These tests pin the behaviour of the deprecated ModbusCommandItem until its removal.
// Remove with ModbusCommandItem before 2027.3.0.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace esphome::modbus_controller::testing {

// The coil write factory packs into an exact-size payload. Pinned at one past the protocol maximum
// because a fixed pack buffer sized for the maximum would silently truncate there while the quantity
// field still claimed every coil - and the truncated frame would fit the RTU limit and go on the wire
// malformed. Built at its true byte count, the oversize frame is refused by the hub's size check with
// a log instead.
TEST(ModbusCommandPayload, CoilWritePayloadIsExactSizedNotTruncated) {
  ModbusController controller(nullptr, 1);
  std::vector<bool> coils(modbus::MAX_NUM_OF_COILS_TO_WRITE + 1, true);
  auto cmd = ModbusCommandItem::create_write_multiple_coils(&controller, 0x10, coils);
  EXPECT_EQ(cmd.payload.size(), modbus::packed_bit_bytes(coils.size()));
}

// LSB-first packing with zeroed pad bits, matching the wire layout the PDU builders produce.
TEST(ModbusCommandPayload, CoilWritePacksLsbFirstWithZeroPad) {
  ModbusController controller(nullptr, 1);
  const std::vector<bool> coils{true, false, true, true};
  auto cmd = ModbusCommandItem::create_write_multiple_coils(&controller, 0x10, coils);
  ASSERT_EQ(cmd.payload.size(), 1u);
  EXPECT_EQ(cmd.payload.data()[0], 0b00001101);
}

}  // namespace esphome::modbus_controller::testing

#pragma GCC diagnostic pop
