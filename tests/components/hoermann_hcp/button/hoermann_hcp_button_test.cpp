#include <gtest/gtest.h>

#include "esphome/components/hoermann_hcp/button/hoermann_hcp_button.h"

#include "../common.h"

namespace esphome::hoermann_hcp::testing {

// The intermediate positions are named in the second register, next to 0x0100 in the first.
TEST(HoermannHcpButtonTest, VentButtonSendsTheVentCommand) {
  TestableHoermannHcp door;
  HoermannHcpVentButton vent(&door);
  connect_controller(door);

  vent.press();

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
  HoermannHcpHalfOpenButton half_open(&door);
  connect_controller(door);

  half_open.press();

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
  HoermannHcpVentButton vent(&door);
  connect_controller(door);
  door.set_position(0.5f);
  consume_command(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0014, 0x0100}));
  ASSERT_EQ(door.get_door_state(), DoorState::OPENING);

  vent.press();
  consume_command(door);

  // Position 120/200 = 0.6 is past the abandoned target, which must no longer stop the door.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// A button carries no state, so a refused press is simply dropped rather than fired once the controller
// turns up, which could be much later.
TEST(HoermannHcpButtonTest, PressWithoutABusControllerSendsNothing) {
  HoermannHcp door;  // never contacted by a bus controller
  HoermannHcpVentButton vent(&door);

  vent.press();

  EXPECT_EQ(poll_command(door).first, 0x0000);
}

}  // namespace esphome::hoermann_hcp::testing
