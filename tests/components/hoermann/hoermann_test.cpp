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

// Exposes the connection bookkeeping so the timeout cleanup can be driven without waiting it out.
class TestableHoermann : public Hoermann {
 public:
  using Hoermann::set_valid_;
};

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
  // Refused while one is pending: were it accepted, the release below would carry COMMAND_CLOSE's 0x0120.
  door.close_door();

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

// Losing the controller must drop a command it never fetched, otherwise it blocks every later command
// and fires unasked once the bus comes back.
TEST(HoermannReadWrite, ConnectionLossDropsThePendingCommand) {
  TestableHoermann door;
  connect(door);
  door.open_door();
  ASSERT_TRUE(door.is_valid());

  door.set_valid_(false);
  EXPECT_FALSE(door.is_valid());

  // The reconnecting poll must not replay the dropped command.
  EXPECT_EQ(poll_command(door), 0x0000);
  // And the slot is free, so a new command is accepted.
  door.close_door();
  EXPECT_EQ(poll_command(door), 0x0220);
}

// The 0x17 read half echoes the message counter and command byte written to COMMAND_REG, packed
// differently per block length.
TEST(HoermannReadWrite, CommandRegisterIsEchoedBack) {
  Hoermann door;
  // Counter 0x34 in the high byte, command 0x07 in the low byte.
  door.on_write_registers(COMMAND_REG, make_registers({0x3407, 0x0000}));

  RegisterValues command_poll;
  door.on_read_holding_registers(STATE_REG, 8, command_poll);
  ASSERT_EQ(command_poll.size(), 8u);
  EXPECT_EQ(command_poll[0], 0x3400);  // counter alone
  EXPECT_EQ(command_poll[1], 0x0701);  // command in the high byte, status 0x01 in the low

  RegisterValues empty_poll;
  door.on_read_holding_registers(STATE_REG, 2, empty_poll);
  ASSERT_EQ(empty_poll.size(), 2u);
  EXPECT_EQ(empty_poll[0], 0x3404);  // status 0x04 shares the register with the counter here
  EXPECT_EQ(empty_poll[1], 0x0700);  // command alone

  RegisterValues scan;
  door.on_read_holding_registers(STATE_REG, 5, scan);
  ASSERT_EQ(scan.size(), 5u);
  EXPECT_EQ(scan[0], 0x3400);
  EXPECT_EQ(scan[1], 0x0705);
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

// A door parking a count short of its end stop must still report exactly closed or open, because
// Cover::is_fully_closed() compares against 0.0 exactly.
TEST(HoermannWrite, EndStopsReportExactPositions) {
  Hoermann door;
  // Position register 1 of 200 while the door reports itself closed.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0001, 0x4000}));
  ASSERT_EQ(door.get_door_state(), DoorState::CLOSED);
  EXPECT_FLOAT_EQ(door.get_current_position(), 0.0f);

  // Position register 199 of 200 while the door reports itself open.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x00C7, 0x2000}));
  ASSERT_EQ(door.get_door_state(), DoorState::OPEN);
  EXPECT_FLOAT_EQ(door.get_current_position(), 1.0f);

  // Away from the end stops the raw count is reported as-is.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0100}));
  EXPECT_FLOAT_EQ(door.get_current_position(), 0.5f);
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

// An impulse restarts a stopped door, so a frame reporting the stop and the target crossing at once
// must be read as "already stopped" rather than "still opening".
TEST(HoermannPosition, StopReportedWithTheCrossingSendsNoImpulse) {
  Hoermann door;
  connect(door);
  door.set_position(0.5f);
  EXPECT_EQ(poll_command(door), 0x0210);
  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  EXPECT_EQ(poll_command(door), 0x0110);

  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0014, 0x0100}));
  ASSERT_EQ(door.get_door_state(), DoorState::OPENING);

  // Same frame: position 0.6 (past the target) and state 0x20 -> the door has reached its open end stop.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x2000}));
  ASSERT_EQ(door.get_door_state(), DoorState::OPEN);
  EXPECT_EQ(poll_command(door), 0x0000);
}

// A target the door never reaches is dropped once it comes to rest, so a later move is not cut short.
TEST(HoermannPosition, TargetIsDroppedWhenTheDoorStopsShort) {
  Hoermann door;
  connect(door);
  door.set_position(0.5f);
  EXPECT_EQ(poll_command(door), 0x0210);
  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  EXPECT_EQ(poll_command(door), 0x0110);

  // The door is stopped at 0.3 by a wall button, short of the requested 0.5.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0014, 0x0100}));
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0000}));
  ASSERT_EQ(door.get_door_state(), DoorState::STOPPED);

  // A later manual open must run freely instead of being stopped at the abandoned target.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0050, 0x0100}));
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  EXPECT_EQ(poll_command(door), 0x0000);
}

// The low byte of broadcast register 6 carries the light state (0x10 and 0x14 mean on).
TEST(HoermannWrite, BroadcastUpdatesLight) {
  Hoermann door;
  auto status = door.on_write_registers(BROADCAST_REG, make_registers({0, 0, 0, 0, 0, 0, 0x0010, 0, 0}));
  EXPECT_FALSE(status.has_value());
  EXPECT_TRUE(door.is_light_on());
}

}  // namespace esphome::hoermann
