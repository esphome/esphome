#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "esphome/components/modbus_controller/modbus_controller.h"

namespace esphome::modbus_controller::testing {

// The coil write factory stores the full WRITE_MULTIPLE_COILS PDU in payload (function code, start
// address, quantity, byte count, then the packed coil bytes), the same convention as the other write
// factories. A request past the protocol maximum is refused outright - payload stays empty so the hub
// drops it - rather than truncating the coils while the quantity field still claims every one.
TEST(ModbusCommandPayload, CoilWriteOverMaxIsRejected) {
  ModbusController controller;
  std::vector<bool> coils(modbus::MAX_NUM_OF_COILS_TO_WRITE + 1, true);
  auto cmd = ModbusCommandItem::create_write_multiple_coils(&controller, 0x10, coils);
  EXPECT_EQ(cmd.payload.size(), 0u);
}

// LSB-first packing with zeroed pad bits, inside the full PDU the write factory builds.
TEST(ModbusCommandPayload, CoilWritePacksLsbFirstWithZeroPad) {
  ModbusController controller;
  const std::vector<bool> coils{true, false, true, true};
  auto cmd = ModbusCommandItem::create_write_multiple_coils(&controller, 0x10, coils);
  // fc, start address (0x0010), quantity (4), byte count (1), then the LSB-first packed coil byte.
  const std::vector<uint8_t> expected{0x0F, 0x00, 0x10, 0x00, 0x04, 0x01, 0b00001101};
  ASSERT_EQ(cmd.payload.size(), expected.size());
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), cmd.payload.data()));
}

}  // namespace esphome::modbus_controller::testing
