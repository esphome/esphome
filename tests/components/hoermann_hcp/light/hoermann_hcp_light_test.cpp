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

// Drives the platform against a real LightState. ALWAYS_OFF keeps setup() clear of preferences.
struct LightFixture {
  HoermannHcp door;
  HoermannHcpLight output{&door};
  light::LightState state{&output};

  LightFixture() {
    this->state.set_restore_mode(light::LIGHT_ALWAYS_OFF);
    this->output.setup();
    this->state.setup();
    this->state.loop();
  }

  // Issues a command the way Home Assistant would, then lets the state machine settle.
  void command(bool on) {
    auto call = this->state.make_call();
    call.set_state(on);
    call.perform();
    for (int i = 0; i < 4; i++)
      this->state.loop();
  }

  // Delivers a lamp broadcast and runs the hub's notification pass.
  void report_lamp(bool on) {
    this->door.on_write_registers(BROADCAST_REG, lamp_broadcast(on ? 0x0010 : 0x0000));
    this->door.update();
    for (int i = 0; i < 4; i++)
      this->state.loop();
  }

  bool entity_on() { return this->state.remote_values.is_on(); }
};

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

// The lamp command is the only one that drives the second command register, and the hub sends it whatever
// the lamp is currently doing.
TEST(HoermannHcpLightTest, LampCommandUsesTheSecondRegister) {
  HoermannHcp door;
  connect(door);
  ASSERT_FALSE(door.is_light_on());
  ASSERT_TRUE(door.toggle_light());

  auto [pressed, pressed_2] = poll_command(door);
  EXPECT_EQ(pressed, 0x0100);
  EXPECT_EQ(pressed_2, 0x0200);
}

// Without a bus controller the command cannot be delivered, and the caller is told.
TEST(HoermannHcpLightTest, LampCommandIsRefusedWhileDisconnected) {
  HoermannHcp door;
  EXPECT_FALSE(door.toggle_light());
}

// Switching the entity on sends one toggle, and the door's own report does not send a second.
TEST(HoermannHcpLightPlatformTest, CommandTogglesOnceAndSettles) {
  LightFixture fixture;
  connect(fixture.door);

  fixture.command(true);
  auto [pressed, pressed_2] = poll_command(fixture.door);
  EXPECT_EQ(pressed, 0x0100);
  EXPECT_EQ(pressed_2, 0x0200);

  // The lamp is now on, and the resulting broadcast must not queue another toggle.
  fixture.report_lamp(true);
  EXPECT_TRUE(fixture.entity_on());
  auto [idle, idle_2] = poll_command(fixture.door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
}

// A lamp switched on at the door itself has to reach the entity.
TEST(HoermannHcpLightPlatformTest, DoorDrivenChangeReachesTheEntity) {
  LightFixture fixture;
  connect(fixture.door);
  ASSERT_FALSE(fixture.entity_on());

  fixture.report_lamp(true);
  EXPECT_TRUE(fixture.entity_on());

  fixture.report_lamp(false);
  EXPECT_FALSE(fixture.entity_on());
}

// A refused command must leave the entity showing the lamp, not the request.
TEST(HoermannHcpLightPlatformTest, RefusedCommandRepublishesTheLamp) {
  LightFixture fixture;  // never connected, so the hub refuses every command

  fixture.command(true);
  EXPECT_FALSE(fixture.entity_on());
}

// A reversing press before the toggle is fetched cancels it, so the lamp never moves.
TEST(HoermannHcpLightPlatformTest, ReversingPressCancelsTheQueuedToggle) {
  LightFixture fixture;
  connect(fixture.door);

  fixture.command(true);
  ASSERT_TRUE(fixture.door.is_light_toggle_pending());

  fixture.command(false);
  EXPECT_FALSE(fixture.door.is_light_toggle_pending());
  EXPECT_FALSE(fixture.entity_on());

  // Nothing is left for the controller to fetch, so the lamp stays off as asked.
  auto [pressed, pressed_2] = poll_command(fixture.door);
  EXPECT_EQ(pressed, 0x0000);
  EXPECT_EQ(pressed_2, 0x0000);
}

}  // namespace esphome::hoermann_hcp
