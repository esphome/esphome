#include <gtest/gtest.h>

#include <utility>

#include "esphome/components/hoermann_hcp/light/hoermann_hcp_light.h"

namespace esphome::hoermann_hcp {

using modbus::RegisterValues;

namespace {

constexpr uint16_t COMMAND_REG = 0x9C41;
constexpr uint16_t STATE_REG = 0x9CB9;
constexpr uint16_t BROADCAST_REG = 0x9D31;

RegisterValues make_registers(std::initializer_list<uint16_t> values) {
  RegisterValues registers;
  for (uint16_t value : values)
    registers.push_back(value);
  return registers;
}

// The door only accepts commands once the bus controller has actually talked to it.
void connect(HoermannHcp &door) { door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000})); }

// Runs one command poll (write 2 / read 8) and returns both key-press registers.
std::pair<uint16_t, uint16_t> poll_command(HoermannHcp &door) {
  door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000}));
  RegisterValues response;
  door.on_read_holding_registers(STATE_REG, 8, response);
  EXPECT_EQ(response.size(), 8u);
  if (response.size() != 8u)
    return {0xFFFF, 0xFFFF};
  return {response[2], response[3]};
}

// A status broadcast carrying the lamp register, which the door reports at index 6.
RegisterValues lamp_broadcast(uint16_t lamp_reg) {
  return make_registers({0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, lamp_reg});
}

}  // namespace

// The lamp state lives in the low byte of register 6; only 0x14 and 0x10 mean lit.
TEST(HoermannHcpLightTest, LampStateIsDecodedFromTheBroadcast) {
  HoermannHcp door;
  EXPECT_FALSE(door.is_light_on());

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0014));
  EXPECT_TRUE(door.is_light_on());

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  EXPECT_FALSE(door.is_light_on());
}

// The lamp command is the only one that drives the second command register.
TEST(HoermannHcpLightTest, LampCommandUsesTheSecondRegister) {
  HoermannHcp door;
  connect(door);
  ASSERT_TRUE(door.toggle_light());

  auto [pressed, pressed_2] = poll_command(door);
  EXPECT_EQ(pressed, 0x0100);
  EXPECT_EQ(pressed_2, 0x0200);
}

// The door offers only a toggle, so a request that already matches must queue nothing.
TEST(HoermannHcpLightTest, RequestMatchingTheLampQueuesNothing) {
  HoermannHcp door;
  connect(door);
  ASSERT_FALSE(door.is_light_on());

  EXPECT_TRUE(door.turn_light(false));
  auto [pressed, pressed_2] = poll_command(door);
  EXPECT_EQ(pressed, 0x0000);
  EXPECT_EQ(pressed_2, 0x0000);
}

// A request that differs from the lamp state is sent as a toggle.
TEST(HoermannHcpLightTest, RequestDifferingFromTheLampSendsAToggle) {
  HoermannHcp door;
  connect(door);

  EXPECT_TRUE(door.turn_light(true));
  auto [pressed, pressed_2] = poll_command(door);
  EXPECT_EQ(pressed, 0x0100);
  EXPECT_EQ(pressed_2, 0x0200);
}

// Without a bus controller the command cannot be delivered, and the caller is told.
TEST(HoermannHcpLightTest, LampCommandIsRefusedWhileDisconnected) {
  HoermannHcp door;
  EXPECT_FALSE(door.turn_light(true));
  EXPECT_FALSE(door.toggle_light());
}

}  // namespace esphome::hoermann_hcp
