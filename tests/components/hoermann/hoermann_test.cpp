#include <gtest/gtest.h>

#include "esphome/components/hoermann/hoermann.h"

namespace esphome::hoermann {

using modbus::RegisterValues;

namespace {

// Register block addresses the Hoermann bus controller polls (see hoermann.cpp).
constexpr uint16_t COMMAND_REG = 0x9C41;
constexpr uint16_t STATE_REG = 0x9CB9;
constexpr uint16_t BROADCAST_REG = 0x9D31;

RegisterValues make_registers(std::initializer_list<uint16_t> values) {
  RegisterValues registers;
  for (uint16_t value : values)
    registers.push_back(value);
  return registers;
}

}  // namespace

// An empty poll (write 2 / read 2) answers with the fixed status word 0x0004.
TEST(HoermannReadWrite, EmptyPollReturnsStatusWord) {
  Hoermann door;
  RegisterValues response;
  auto status =
      door.on_modbus_read_write_registers(STATE_REG, 2, COMMAND_REG, make_registers({0x0000, 0x0000}), response);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(response.size(), 2u);
  EXPECT_EQ(response[0], 0x0004);
  EXPECT_EQ(response[1], 0x0000);
}

// A bus scan (write 3 / read 5) answers with the fixed device identification block.
TEST(HoermannReadWrite, BusScanReturnsIdentification) {
  Hoermann door;
  RegisterValues response;
  auto status = door.on_modbus_read_write_registers(STATE_REG, 5, COMMAND_REG, make_registers({0x0000, 0x0000, 0x0000}),
                                                    response);
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
  RegisterValues response;
  auto status =
      door.on_modbus_read_write_registers(STATE_REG, 8, COMMAND_REG, make_registers({0x0000, 0x0000}), response);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(response.size(), 8u);
  EXPECT_EQ(response[1], 0x0001);
  EXPECT_EQ(response[2], 0x0000);
  EXPECT_EQ(response[3], 0x0000);
}

// A queued control command is injected into the next command poll as a simulated key press.
TEST(HoermannReadWrite, QueuedCommandIsInjectedIntoPoll) {
  Hoermann door;
  door.open_door();
  RegisterValues response;
  auto status =
      door.on_modbus_read_write_registers(STATE_REG, 8, COMMAND_REG, make_registers({0x0000, 0x0000}), response);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(response.size(), 8u);
  EXPECT_EQ(response[2], 0x0210);  // COMMAND_OPEN "key pressed" value
  EXPECT_EQ(response[3], 0x0000);
}

// A status broadcast (function code 0x10 to 0x9D31) updates the decoded door state and position.
TEST(HoermannWrite, BroadcastUpdatesStateAndPosition) {
  Hoermann door;
  // registers[1] low byte = position (value / 200), registers[2] high byte = state (0x01 -> opening).
  auto status = door.on_modbus_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0100}));
  EXPECT_FALSE(status.has_value());
  EXPECT_EQ(door.get_door_state(), DoorState::OPENING);
  EXPECT_FLOAT_EQ(door.get_current_position(), 0.5f);
}

}  // namespace esphome::hoermann
