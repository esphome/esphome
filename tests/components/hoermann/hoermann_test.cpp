#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "esphome/components/hoermann/hoermann.h"

namespace esphome::hoermann {

using modbus::RegisterValues;

namespace {

// Register block addresses the Hoermann bus controller polls (see hoermann.cpp).
constexpr uint16_t COMMAND_REG = 0x9C41;
constexpr uint16_t STATE_REG = 0x9CB9;
constexpr uint16_t BROADCAST_REG = 0x9D31;

// Simulated key-press duration in hoermann.cpp, plus margin for a loaded CI runner.
constexpr auto KEY_PRESS_ELAPSED = std::chrono::milliseconds(150);

RegisterValues make_registers(std::initializer_list<uint16_t> values) {
  RegisterValues registers;
  for (uint16_t value : values)
    registers.push_back(value);
  return registers;
}

// The device only accepts commands once the bus controller has actually talked to it.
void connect(Hoermann &door) { door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000})); }

// Runs one command poll (write 2 / read 8) and returns the register carrying the key-press value.
uint16_t poll_command(Hoermann &door) {
  door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000}));
  RegisterValues response;
  door.on_read_holding_registers(STATE_REG, 8, response);
  EXPECT_EQ(response.size(), 8u);
  return response.size() == 8u ? response[2] : 0xFFFF;
}

}  // namespace

// An empty poll (write 2 / read 2) answers with the fixed status word 0x0004.
TEST(HoermannReadWrite, EmptyPollReturnsStatusWord) {
  Hoermann door;
  EXPECT_FALSE(door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000})).has_value());
  RegisterValues response;
  auto status = door.on_read_holding_registers(STATE_REG, 2, response);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(response.size(), 2u);
  EXPECT_EQ(response[0], 0x0004);
  EXPECT_EQ(response[1], 0x0000);
}

// A bus scan (write 3 / read 5) answers with the fixed device identification block.
TEST(HoermannReadWrite, BusScanReturnsIdentification) {
  Hoermann door;
  EXPECT_FALSE(door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000, 0x0000})).has_value());
  RegisterValues response;
  auto status = door.on_read_holding_registers(STATE_REG, 5, response);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(response.size(), 5u);
  EXPECT_EQ(response[1], 0x0005);
  EXPECT_EQ(response[2], 0x0430);
  EXPECT_EQ(response[3], 0x10ff);
  EXPECT_EQ(response[4], 0xa845);
}

// Without a queued command, the command poll (write 2 / read 8) reports idle and no key press.
TEST(HoermannReadWrite, IdleCommandPollHasNoCommand) {
  Hoermann door;
  EXPECT_FALSE(door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000})).has_value());
  RegisterValues response;
  auto status = door.on_read_holding_registers(STATE_REG, 8, response);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(response.size(), 8u);
  EXPECT_EQ(response[1], 0x0001);
  EXPECT_EQ(response[2], 0x0000);
  EXPECT_EQ(response[3], 0x0000);
}

// A queued control command is injected into the next command poll as a simulated key press.
TEST(HoermannReadWrite, QueuedCommandIsInjectedIntoPoll) {
  Hoermann door;
  connect(door);
  door.open_door();
  EXPECT_FALSE(door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000})).has_value());
  RegisterValues response;
  auto status = door.on_read_holding_registers(STATE_REG, 8, response);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(response.size(), 8u);
  EXPECT_EQ(response[2], 0x0210);  // COMMAND_OPEN "key pressed" value
  EXPECT_EQ(response[3], 0x0000);
}

// A read of any other block is an addressing error rather than a successful all-zero reply.
TEST(HoermannReadWrite, UnknownAddressIsRejected) {
  Hoermann door;
  RegisterValues response;
  EXPECT_EQ(door.on_read_holding_registers(0x1234, 2, response), modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS);
  EXPECT_EQ(door.on_write_registers(0x1234, make_registers({0x0000})), modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS);
}

// A command is held for the key-press duration, then released, and only then can the next one be queued.
TEST(HoermannReadWrite, CommandIsReleasedAfterTheKeyPressDelay) {
  Hoermann door;
  connect(door);
  door.open_door();
  EXPECT_EQ(poll_command(door), 0x0210);  // COMMAND_OPEN pressed
  // A second command is refused while the first one is still being presented.
  door.close_door();
  EXPECT_EQ(poll_command(door), 0x0000);  // still within the key-press window

  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  EXPECT_EQ(poll_command(door), 0x0110);  // COMMAND_OPEN released
  // With the command gone, the next one is accepted again.
  door.close_door();
  EXPECT_EQ(poll_command(door), 0x0220);  // COMMAND_CLOSE pressed
}

// Commands issued while the bus controller is absent are dropped instead of firing when it returns.
TEST(HoermannReadWrite, CommandIsDroppedWhileDisconnected) {
  Hoermann door;
  door.open_door();
  EXPECT_EQ(poll_command(door), 0x0000);
}

// A status broadcast (function code 0x10 to 0x9D31) updates the decoded door state and position.
TEST(HoermannWrite, BroadcastUpdatesStateAndPosition) {
  Hoermann door;
  // registers[1] low byte = position (value / 200), registers[2] high byte = state (0x01 -> opening).
  auto status = door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0100}));
  EXPECT_FALSE(status.has_value());
  EXPECT_EQ(door.get_door_state(), DoorState::OPENING);
  EXPECT_FLOAT_EQ(door.get_current_position(), 0.5f);
}

// The vent position is reported as state 0x00 with low byte 0x61, so a change confined to the low byte of
// the state register still has to be decoded.
TEST(HoermannWrite, VentIsDecodedFromTheStateLowByte) {
  Hoermann door;
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x0100}));
  ASSERT_EQ(door.get_door_state(), DoorState::OPENING);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x0000}));
  ASSERT_EQ(door.get_door_state(), DoorState::STOPPED);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x0061}));
  EXPECT_EQ(door.get_door_state(), DoorState::VENT);
}

// A position request below the lower snap threshold becomes a plain close command.
TEST(HoermannPosition, NearlyClosedTargetClosesTheDoor) {
  Hoermann door;
  connect(door);
  door.set_position(0.02f);
  RegisterValues response;
  door.on_read_holding_registers(STATE_REG, 8, response);
  ASSERT_EQ(response.size(), 8u);
  EXPECT_EQ(response[2], 0x0220);  // COMMAND_CLOSE "key pressed" value
}

// A half-open target starts the door moving towards the requested position.
TEST(HoermannPosition, HalfOpenTargetOpensTheDoor) {
  Hoermann door;  // starts out fully closed
  connect(door);
  door.set_position(0.5f);
  RegisterValues response;
  door.on_read_holding_registers(STATE_REG, 8, response);
  ASSERT_EQ(response.size(), 8u);
  EXPECT_EQ(response[2], 0x0210);  // COMMAND_OPEN "key pressed" value
}

// The door has no notion of a target, so it is stopped with an impulse once it travels past the request.
TEST(HoermannPosition, TargetPositionStopsTheDoor) {
  Hoermann door;
  connect(door);
  door.set_position(0.5f);
  EXPECT_EQ(poll_command(door), 0x0210);  // COMMAND_OPEN pressed
  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  EXPECT_EQ(poll_command(door), 0x0110);  // COMMAND_OPEN released

  // Position 20/200 = 0.1 while opening: short of the target, so the door keeps going.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0014, 0x0100}));
  ASSERT_EQ(door.get_door_state(), DoorState::OPENING);
  EXPECT_EQ(poll_command(door), 0x0000);

  // Position 120/200 = 0.6 is past the target, so the door is stopped.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  EXPECT_EQ(poll_command(door), 0x0240);  // COMMAND_IMPULSE pressed
}

}  // namespace esphome::hoermann
