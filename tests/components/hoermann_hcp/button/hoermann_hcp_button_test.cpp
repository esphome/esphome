#include <gtest/gtest.h>

#include "../common.h"

namespace esphome::hoermann_hcp::testing {

// The buttons press straight into vent_door() and half_open_door().
// The intermediate positions are named in the second register, next to 0x0100 in the first.
TEST(HoermannHcpButtonTest, VentButtonSendsTheVentCommand) {
  TestableHoermannHcp door;
  connect_controller(door);

  door.vent_door();

  auto [value, value_2] = poll_command(door);
  EXPECT_EQ(value, 0x0100);
  EXPECT_EQ(value_2, 0x4000);
  // Sent once, in the first register as well as in the second.
  auto [after, after_2] = poll_command(door);
  EXPECT_EQ(after, 0x0000);
  EXPECT_EQ(after_2, 0x0000);
}

TEST(HoermannHcpButtonTest, HalfOpenButtonSendsTheHalfOpenCommand) {
  TestableHoermannHcp door;
  connect_controller(door);

  door.half_open_door();

  auto [value, value_2] = poll_command(door);
  EXPECT_EQ(value, 0x0100);
  EXPECT_EQ(value_2, 0x0400);
  // Sent once, in the first register as well as in the second.
  auto [after, after_2] = poll_command(door);
  EXPECT_EQ(after, 0x0000);
  EXPECT_EQ(after_2, 0x0000);
}

// The door drives to the vent position on its own, so a position the cover was still travelling to must not
// stop it on the way there.
TEST(HoermannHcpButtonTest, VentAbandonsAnArmedTarget) {
  TestableHoermannHcp door;  // starts out fully closed
  connect_controller(door);
  door.set_position(0.5f);
  consume_command(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0014, 0x0100}));
  ASSERT_EQ(door.get_door_state(), DoorState::OPENING);

  door.vent_door();
  consume_command(door);

  // Position 120/200 = 0.6 is past the abandoned target, which must no longer stop the door.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// A button carries no state, so a refused press is simply dropped rather than fired once the controller
// turns up, which could be much later.
TEST(HoermannHcpButtonTest, PressWithoutABusControllerSendsNothing) {
  HoermannHcp door;  // never contacted by a bus controller

  door.vent_door();

  EXPECT_EQ(poll_command(door).first, 0x0000);
}

}  // namespace esphome::hoermann_hcp::testing
