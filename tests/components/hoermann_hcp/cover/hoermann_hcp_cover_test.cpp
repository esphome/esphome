#include <gtest/gtest.h>

#include "esphome/components/hoermann_hcp/cover/hoermann_hcp_cover.h"

#include "../common.h"

namespace esphome::hoermann_hcp::testing {

// Cover::position starts at COVER_OPEN, so a door that is already closed still has a state to publish.
TEST(HoermannHcpCoverTest, ClosedDoorPublishesItsInitialPosition) {
  HoermannHcp door;
  HoermannHcpCover cover(&door);
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
TEST(HoermannHcpCoverTest, DirectionlessMoveHoldsTheOperationUntilThePositionMoves) {
  HoermannHcp door;
  HoermannHcpCover cover(&door);
  cover.setup();

  // Position 100/200 = 0.5, state 0x80 -> resting half open.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x8000}));
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

// Booting while the door is already mid-move gives no baseline to compare against, so no direction
// may be inferred from the first update.
TEST(HoermannHcpCoverTest, FirstDirectionlessMoveDoesNotGuessADirection) {
  HoermannHcp door;
  HoermannHcpCover cover(&door);
  cover.setup();

  // The very first thing seen is a half-open move already at 100/200 = 0.5.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0500}));
  door.update();
  EXPECT_EQ(cover.current_operation, cover::COVER_OPERATION_IDLE);
}

// A cover.open arrives as a position of 1.0, so it has to reach the door as a plain open command rather
// than as a target the door would be stopped at.
TEST(HoermannHcpCoverTest, OpenCommandOpensTheDoor) {
  HoermannHcp door;
  HoermannHcpCover cover(&door);
  cover.setup();
  connect_controller(door);

  cover.make_call().set_command_open().perform();
  EXPECT_EQ(poll_command(door).first, 0x0210);  // COMMAND_OPEN pressed
}

// The same for cover.close, which arrives as a position of 0.0.
TEST(HoermannHcpCoverTest, CloseCommandClosesTheDoor) {
  HoermannHcp door;
  HoermannHcpCover cover(&door);
  cover.setup();
  connect_controller(door);

  cover.make_call().set_command_close().perform();
  EXPECT_EQ(poll_command(door).first, 0x0220);  // COMMAND_CLOSE pressed
}

TEST(HoermannHcpCoverTest, ToggleCommandSendsAnImpulse) {
  HoermannHcp door;
  HoermannHcpCover cover(&door);
  cover.setup();
  connect_controller(door);

  cover.make_call().set_command_toggle().perform();
  EXPECT_EQ(poll_command(door).first, 0x0240);  // COMMAND_IMPULSE pressed
}

TEST(HoermannHcpCoverTest, StopCommandStopsAMovingDoor) {
  HoermannHcp door;
  HoermannHcpCover cover(&door);
  cover.setup();
  connect_controller(door);
  // The door is opening, so it takes an impulse to stop it.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0100}));

  cover.make_call().set_command_stop().perform();
  EXPECT_EQ(poll_command(door).first, 0x0240);  // COMMAND_IMPULSE pressed
}

// A position between the end stops starts the door in the right direction; it is stopped there later.
TEST(HoermannHcpCoverTest, PositionCommandStartsTheDoorTowardsTheTarget) {
  HoermannHcp door;  // starts out fully closed
  HoermannHcpCover cover(&door);
  cover.setup();
  connect_controller(door);

  cover.make_call().set_position(0.5f).perform();
  EXPECT_EQ(poll_command(door).first, 0x0210);  // COMMAND_OPEN pressed
}

// A command the door cannot take is assumed to have worked by whoever sent it, so the unchanged state has
// to be published back over that assumption.
TEST(HoermannHcpCoverTest, RefusedCommandPublishesTheUnchangedState) {
  HoermannHcp door;  // never contacted by a bus controller
  HoermannHcpCover cover(&door);
  cover.setup();
  int publishes = 0;
  cover.add_on_state_callback([&publishes]() { publishes++; });

  cover.make_call().set_command_close().perform();

  EXPECT_EQ(poll_command(door).first, 0x0000);
  EXPECT_EQ(publishes, 1);
  EXPECT_FLOAT_EQ(cover.position, cover::COVER_OPEN);
}

// Nothing is published before the bus controller is heard from, so a door that never reaches the bus would
// otherwise sit at its fully open default and look healthy.
TEST(HoermannHcpCoverTest, MissingBusControllerIsFlaggedUntilFirstContact) {
  HoermannHcp door;
  HoermannHcpCover cover(&door);
  cover.setup();
  EXPECT_TRUE(cover.status_has_warning());

  connect_controller(door);
  door.update();
  EXPECT_FALSE(cover.status_has_warning());
}

}  // namespace esphome::hoermann_hcp::testing
