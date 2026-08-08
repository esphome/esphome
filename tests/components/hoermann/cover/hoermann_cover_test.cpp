#include <gtest/gtest.h>

#include "esphome/components/hoermann/cover/hoermann_cover.h"

namespace esphome::hoermann {

using modbus::RegisterValues;

namespace {

constexpr uint16_t COMMAND_REG = 0x9C41;
constexpr uint16_t BROADCAST_REG = 0x9D31;

RegisterValues make_registers(std::initializer_list<uint16_t> values) {
  RegisterValues registers;
  for (uint16_t value : values)
    registers.push_back(value);
  return registers;
}

}  // namespace

// Cover::position starts at COVER_OPEN, so a door that is already closed still has a state to publish.
TEST(HoermannCoverTest, ClosedDoorPublishesItsInitialPosition) {
  Hoermann door;
  HoermannCover cover(&door);
  cover.setup();
  int publishes = 0;
  cover.add_on_state_callback([&publishes]() { publishes++; });
  ASSERT_FLOAT_EQ(cover.position, cover::COVER_OPEN);

  // Any request marks the device connected, which is itself a state change.
  door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000}));
  door.update();

  EXPECT_EQ(publishes, 1);
  EXPECT_FLOAT_EQ(cover.position, cover::COVER_CLOSED);
}

// Venting and half-open moves report no direction, so one is only derived once the position has moved.
TEST(HoermannCoverTest, DirectionlessMoveHoldsTheOperationUntilThePositionMoves) {
  Hoermann door;
  HoermannCover cover(&door);
  cover.setup();

  // Position 100/200 = 0.5, state 0x40 -> closed.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x4000}));
  door.update();
  ASSERT_EQ(cover.current_operation, cover::COVER_OPERATION_IDLE);

  // State 0x05 -> moving to half-open, but the position has not moved yet.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0500}));
  door.update();
  EXPECT_EQ(cover.current_operation, cover::COVER_OPERATION_IDLE);

  // Position 120/200 = 0.6 is higher than before, so the door is opening.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0500}));
  door.update();
  EXPECT_EQ(cover.current_operation, cover::COVER_OPERATION_OPENING);
  EXPECT_FLOAT_EQ(cover.position, 0.6f);
}

}  // namespace esphome::hoermann
