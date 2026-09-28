#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "common.h"

namespace esphome::hoermann_hcp::testing {

// An empty poll (write 2 / read 2) answers with the fixed status word 0x0004.
TEST(HoermannHcpReadWrite, EmptyPollReturnsStatusWord) {
  HoermannHcp door;
  EXPECT_FALSE(door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000})).has_value());
  RegisterValues response;
  auto status = door.on_read_holding_registers(STATE_REG, 2, response);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(response.size(), 2u);
  EXPECT_EQ(response[0], 0x0004);
  EXPECT_EQ(response[1], 0x0000);
}

// A bus scan (write 3 / read 5) answers with the fixed device identification block.
TEST(HoermannHcpReadWrite, BusScanReturnsIdentification) {
  HoermannHcp door;
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

// Without a queued command, the command poll (write 2 / read 8) reports idle and no command.
TEST(HoermannHcpReadWrite, IdleCommandPollHasNoCommand) {
  HoermannHcp door;
  EXPECT_FALSE(door.on_write_registers(COMMAND_REG, make_registers({0x0003, 0x0000})).has_value());
  RegisterValues response;
  auto status = door.on_read_holding_registers(STATE_REG, 8, response);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(response.size(), 8u);
  EXPECT_EQ(response[1], 0x0301);
  EXPECT_EQ(response[2], 0x0000);
  EXPECT_EQ(response[3], 0x0000);
}

// A queued control command is injected into the next command poll.
TEST(HoermannHcpReadWrite, QueuedCommandIsInjectedIntoPoll) {
  HoermannHcp door;
  connect_controller(door);
  door.open_door();
  EXPECT_FALSE(door.on_write_registers(COMMAND_REG, make_registers({0x0003, 0x0000})).has_value());
  RegisterValues response;
  auto status = door.on_read_holding_registers(STATE_REG, 8, response);
  EXPECT_FALSE(status.has_value());
  ASSERT_EQ(response.size(), 8u);
  EXPECT_EQ(response[2], 0x0110);  // COMMAND_OPEN
  EXPECT_EQ(response[3], 0x0000);
}

// A read of any other block is an addressing error rather than a successful all-zero reply.
TEST(HoermannHcpReadWrite, UnknownAddressIsRejected) {
  HoermannHcp door;
  RegisterValues response;
  EXPECT_EQ(door.on_read_holding_registers(0x1234, 2, response), modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS);
  EXPECT_EQ(door.on_write_registers(0x1234, make_registers({0x0000})), modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS);
}

// A door command goes out in one answer and frees the slot at once.
TEST(HoermannHcpReadWrite, DoorCommandIsSentOnceAndFreesTheSlot) {
  TestableHoermannHcp door;
  connect_controller(door);
  EXPECT_TRUE(door.open_door());
  // Refused while one is unfetched.
  EXPECT_FALSE(door.close_door());

  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN
  EXPECT_EQ(poll_command(door).first, 0x0000);
  // Once the door has run and come to rest, the slot takes the next command.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0100}));
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x00C8, 0x2000}));
  EXPECT_TRUE(door.close_door());
  EXPECT_EQ(poll_command(door).first, 0x0120);  // COMMAND_CLOSE
}

// A moving door is only ever stopped, whatever it is asked to do.
TEST(HoermannHcpReadWrite, MovingDoorIsOnlyStopped) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_EQ(door.get_door_state(), DoorState::OPENING);

  EXPECT_TRUE(door.close_door());
  EXPECT_EQ(poll_command(door).first, 0x0140);  // COMMAND_IMPULSE
}

// A second stop within 500 ms is the same press and must not restart the door.
TEST(HoermannHcpReadWrite, SecondStopWithinHalfASecondIsIgnored) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_TRUE(door.stop_door());
  EXPECT_EQ(poll_command(door).first, 0x0140);  // COMMAND_IMPULSE

  // Still reported moving while it slows down.
  EXPECT_TRUE(door.open_door());
  EXPECT_EQ(poll_command(door).first, 0x0000);

  door.last_stop_at_ -= 500;
  EXPECT_TRUE(door.stop_door());
  EXPECT_EQ(poll_command(door).first, 0x0140);
}

// A stop outranks a command still waiting, so a door at rest does not start after it.
TEST(HoermannHcpReadWrite, StopCancelsAnUnfetchedCommand) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x4000}));
  ASSERT_TRUE(door.open_door());
  EXPECT_TRUE(door.stop_door());
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// The impulse would start a door that came to rest while the stop waited, so the stop is dropped instead.
TEST(HoermannHcpReadWrite, StopIsDroppedWhenTheDoorRestsBeforeTheFetch) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_TRUE(door.stop_door());
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x00C8, 0x2000}));
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// Until the door reports the start it reads as at rest, so a command in between only stops it, once it moves.
TEST(HoermannHcpReadWrite, CommandBeforeTheStartIsReportedBecomesAStop) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x4000}));
  ASSERT_TRUE(door.open_door());
  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN

  EXPECT_TRUE(door.close_door());
  EXPECT_EQ(poll_command(door).first, 0x0000);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0004, 0x0100}));
  EXPECT_EQ(poll_command(door).first, 0x0140);  // COMMAND_IMPULSE
}

// A door that never reports moving after its command is at rest after all, so later commands are its own.
TEST(HoermannHcpReadWrite, StartWindowClosesWhenTheDoorNeverMoves) {
  TestableHoermannHcp door;
  door.start_window_ms_ = 20;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x4000}));
  ASSERT_TRUE(door.open_door());
  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  connect_controller(door);
  door.update();

  EXPECT_TRUE(door.open_door());
  EXPECT_EQ(poll_command(door).first, 0x0110);
}

// Only the read half of a status poll carries a command, so a second read without a new write gets none.
TEST(HoermannHcpReadWrite, SecondReadWithoutAWriteCarriesNoCommand) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x4000}));
  door.on_write_registers(COMMAND_REG, make_registers({0x0003, 0x0000}));
  RegisterValues first;
  door.on_read_holding_registers(STATE_REG, 8, first);
  ASSERT_TRUE(door.open_door());
  RegisterValues second;
  door.on_read_holding_registers(STATE_REG, 8, second);
  ASSERT_EQ(second.size(), 8u);
  EXPECT_EQ(second[2], 0x0000);
  // The next status poll takes it.
  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN
}

// A command queued for a door at rest only stops it if the door was started from elsewhere before the fetch.
TEST(HoermannHcpReadWrite, CommandForADoorStartedBeforeTheFetchStopsIt) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x00C8, 0x2000}));
  ASSERT_TRUE(door.open_door());
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x00C0, 0x0200}));
  EXPECT_EQ(poll_command(door).first, 0x0140);  // COMMAND_IMPULSE
}

// A stop held for a late start report is sent once the door reports moving.
TEST(HoermannHcpReadWrite, HeldStopSurvivesALateStart) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x4000}));
  ASSERT_TRUE(door.open_door());
  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN
  ASSERT_TRUE(door.stop_door());
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  connect_controller(door);
  door.update();

  // The start is reported after the stop's own fetch deadline, which then starts over.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0004, 0x0100}));
  door.update();
  EXPECT_EQ(poll_command(door).first, 0x0140);  // COMMAND_IMPULSE
}

// A held stop goes when the door never reports its start, and never becomes an impulse.
TEST(HoermannHcpReadWrite, HeldStopIsDroppedWhenTheDoorNeverStarts) {
  TestableHoermannHcp door;
  door.start_window_ms_ = 20;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x4000}));
  ASSERT_TRUE(door.open_door());
  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN
  ASSERT_TRUE(door.stop_door());
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  connect_controller(door);
  door.update();
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// A command for where the door already rests does not move it, so the next command is its own.
TEST(HoermannHcpReadWrite, CommandForTheCurrentEndOpensNoStartWindow) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x00C8, 0x2000}));
  ASSERT_TRUE(door.open_door());
  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN
  ASSERT_TRUE(door.close_door());
  EXPECT_EQ(poll_command(door).first, 0x0120);  // COMMAND_CLOSE
}

// The stop lock ends with the door at rest, so a target reached soon after the next start still stops it.
TEST(HoermannHcpReadWrite, StopLockEndsWhenTheDoorRests) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_TRUE(door.stop_door());
  EXPECT_EQ(poll_command(door).first, 0x0140);  // COMMAND_IMPULSE
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0000}));
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003E, 0x0100}));
  ASSERT_TRUE(door.stop_door());
  EXPECT_EQ(poll_command(door).first, 0x0140);
}

// A stop pressed while disconnected is not kept for the reconnect, where it could start a door at rest.
TEST(HoermannHcpReadWrite, StopWhileDisconnectedIsNotQueued) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  door.update();
  ASSERT_FALSE(door.is_valid());
  ASSERT_EQ(door.get_door_state(), DoorState::OPENING);  // the stale report the stop would be judged by
  EXPECT_FALSE(door.stop_door());
  connect_controller(door);
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// Pressing stop twice before the fetch sends one impulse.
TEST(HoermannHcpReadWrite, SecondStopBeforeTheFetchSendsOneImpulse) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_TRUE(door.stop_door());
  ASSERT_TRUE(door.stop_door());
  EXPECT_EQ(poll_command(door).first, 0x0140);  // COMMAND_IMPULSE
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// A position asked for before the start is reported stops the door once it moves.
TEST(HoermannHcpReadWrite, PositionBeforeTheStartIsReportedIsAStop) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x4000}));
  ASSERT_TRUE(door.open_door());
  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN
  ASSERT_TRUE(door.set_position(0.5f));
  EXPECT_EQ(poll_command(door).first, 0x0000);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0004, 0x0100}));
  EXPECT_EQ(poll_command(door).first, 0x0140);  // COMMAND_IMPULSE
}

// A stop left waiting for a door that came to rest does not block the next command.
TEST(HoermannHcpReadWrite, StaleStopDoesNotBlockTheNextCommand) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x00C0, 0x0100}));
  ASSERT_TRUE(door.stop_door());
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x00C8, 0x2000}));
  EXPECT_TRUE(door.close_door());
  EXPECT_EQ(poll_command(door).first, 0x0120);  // COMMAND_CLOSE
}

// Close on a closed door and vent at the vent position are no moves either, but half open at vent is.
TEST(HoermannHcpReadWrite, EveryEndOpensNoStartWindow) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x4000}));
  ASSERT_TRUE(door.close_door());
  EXPECT_EQ(poll_command(door).first, 0x0120);  // COMMAND_CLOSE
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0010, 0x0A00}));
  ASSERT_EQ(door.get_door_state(), DoorState::VENT);
  ASSERT_TRUE(door.vent_door());
  EXPECT_EQ(poll_command(door).first, 0x0100);  // COMMAND_VENT
  ASSERT_TRUE(door.half_open_door());
  EXPECT_EQ(poll_command(door).first, 0x0100);  // COMMAND_HALF_OPEN
  // Half open from vent is a move, so a stop now waits for its start.
  ASSERT_TRUE(door.stop_door());
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// Until the door has reported where it is, no command counts as a no-op.
TEST(HoermannHcpReadWrite, CommandBeforeTheFirstReportOpensAStartWindow) {
  TestableHoermannHcp door;
  connect_controller(door);
  ASSERT_TRUE(door.close_door());
  EXPECT_EQ(poll_command(door).first, 0x0120);  // COMMAND_CLOSE
  ASSERT_TRUE(door.stop_door());
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0200}));
  EXPECT_EQ(poll_command(door).first, 0x0140);  // COMMAND_IMPULSE
}

// A stop dropped with the start window does not come back when the door moves after all.
TEST(HoermannHcpReadWrite, DroppedHeldStopDoesNotFireLater) {
  TestableHoermannHcp door;
  door.start_window_ms_ = 20;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x4000}));
  ASSERT_TRUE(door.open_door());
  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN
  ASSERT_TRUE(door.stop_door());
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  connect_controller(door);
  door.update();
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0004, 0x0100}));
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// A start does not hold off the stop that follows it.
TEST(HoermannHcpReadWrite, StopRightAfterAStartIsSent) {
  TestableHoermannHcp door;
  connect_controller(door);
  ASSERT_TRUE(door.impulse_door());
  EXPECT_EQ(poll_command(door).first, 0x0140);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));

  EXPECT_TRUE(door.stop_door());
  EXPECT_EQ(poll_command(door).first, 0x0140);
}

// Only a status poll (command 0x03) fetches a command; another 8-register read carries zeros.
TEST(HoermannHcpReadWrite, CommandWaitsForAStatusPoll) {
  TestableHoermannHcp door;
  connect_controller(door);
  ASSERT_TRUE(door.open_door());

  // A transfer write (command 0x04) read back as 8 registers.
  door.on_write_registers(COMMAND_REG, make_registers({0x0504, 0x0000}));
  RegisterValues other;
  door.on_read_holding_registers(STATE_REG, 8, other);
  ASSERT_EQ(other.size(), 8u);
  EXPECT_EQ(other[2], 0x0000);
  EXPECT_EQ(other[3], 0x0000);

  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN
}

// Commands issued while the bus controller is absent are dropped instead of firing when it returns.
TEST(HoermannHcpReadWrite, CommandIsDroppedWhileDisconnected) {
  HoermannHcp door;
  door.open_door();
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// Losing the controller must drop a command it never fetched, otherwise it blocks every later command
// and fires unasked once the bus comes back.
TEST(HoermannHcpReadWrite, ConnectionLossDropsThePendingCommand) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.open_door();
  ASSERT_TRUE(door.is_valid());

  door.set_valid_(false);
  EXPECT_FALSE(door.is_valid());

  // The reconnecting poll must not replay the dropped command.
  EXPECT_EQ(poll_command(door).first, 0x0000);
  // And the slot is free, so a new command is accepted.
  door.close_door();
  EXPECT_EQ(poll_command(door).first, 0x0120);
}

// The connection is dropped by update() once the controller stops polling, which is what releases a
// command it never fetched in the field.
TEST(HoermannHcpReadWrite, PollingTimeoutDropsTheConnection) {
  TestableHoermannHcp door;
  // Wide enough that a stall cannot expire the connection before the check below runs.
  door.connection_timeout_ms_ = 10000;
  connect_controller(door);
  door.open_door();

  // Still inside the window: the controller counts as present.
  door.update();
  ASSERT_TRUE(door.is_valid());

  // Shrink the window so the expiry needs only a short sleep; overshooting it only makes it surer.
  door.connection_timeout_ms_ = 20;
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  door.update();
  EXPECT_FALSE(door.is_valid());
  // The pending command went with the connection instead of firing on the reconnecting poll.
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// Status broadcasts alone keep the connection alive, so a command the controller never fetches has to
// expire on its own; otherwise it blocks every later command until the bus goes quiet entirely.
TEST(HoermannHcpReadWrite, UnfetchedCommandExpiresWhileConnected) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 200;
  connect_controller(door);
  door.open_door();

  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  // A status broadcast refreshes the connection without ever fetching the command.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0000}));
  door.update();
  ASSERT_TRUE(door.is_valid());

  // With the stale command gone, the door accepts commands again.
  door.close_door();
  EXPECT_EQ(poll_command(door).first, 0x0120);
}

// The 0x17 read half echoes the message counter and command byte written to COMMAND_REG, packed
// differently per block length.
TEST(HoermannHcpReadWrite, CommandRegisterIsEchoedBack) {
  HoermannHcp door;
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
TEST(HoermannHcpWrite, BroadcastUpdatesStateAndPosition) {
  HoermannHcp door;
  // registers[1] low byte = position (value / 200), registers[2] high byte = state (0x01 -> opening).
  auto status = door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0100}));
  EXPECT_FALSE(status.has_value());
  EXPECT_EQ(door.get_door_state(), DoorState::OPENING);
  EXPECT_FLOAT_EQ(door.get_current_position(), 0.5f);
}

// The first broadcast has to be decoded even when it carries the register's initial value, otherwise a
// door parked mid-travel at boot keeps the CLOSED default and reports itself fully closed.
TEST(HoermannHcpWrite, FirstBroadcastReportingAStopIsDecoded) {
  HoermannHcp door;
  auto status = door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0000}));
  EXPECT_FALSE(status.has_value());
  EXPECT_EQ(door.get_door_state(), DoorState::STOPPED);
  EXPECT_FLOAT_EQ(door.get_current_position(), 0.5f);
}

// The vent position is reported as state 0x00 with low byte 0x61, so a change confined to the low byte of
// the state register still has to be decoded.
TEST(HoermannHcpWrite, VentIsDecodedFromTheStateLowByte) {
  HoermannHcp door;
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x0100}));
  ASSERT_EQ(door.get_door_state(), DoorState::OPENING);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x0000}));
  ASSERT_EQ(door.get_door_state(), DoorState::STOPPED);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x0061}));
  EXPECT_EQ(door.get_door_state(), DoorState::VENT);
}

// A door parking a count short of its end stop must still report exactly closed or open, because
// Cover::is_fully_closed() compares against 0.0 exactly.
TEST(HoermannHcpWrite, EndStopsReportExactPositions) {
  HoermannHcp door;
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
TEST(HoermannHcpPosition, NearlyClosedTargetClosesTheDoor) {
  HoermannHcp door;
  connect_controller(door);
  door.set_position(0.02f);
  EXPECT_EQ(poll_command(door).first, 0x0120);  // COMMAND_CLOSE
}

// A half-open target starts the door moving towards the requested position.
TEST(HoermannHcpPosition, HalfOpenTargetOpensTheDoor) {
  HoermannHcp door;  // starts out fully closed
  connect_controller(door);
  door.set_position(0.5f);
  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN
}

// The door has no notion of a target, so it is stopped with an impulse once it travels past the request.
TEST(HoermannHcpPosition, TargetPositionStopsTheDoor) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.set_position(0.5f);
  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN

  // Position 20/200 = 0.1 while opening: short of the target, so the door keeps going.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0014, 0x0100}));
  ASSERT_EQ(door.get_door_state(), DoorState::OPENING);
  EXPECT_EQ(poll_command(door).first, 0x0000);

  // Position 120/200 = 0.6 is past the target, so the door is stopped.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  EXPECT_EQ(poll_command(door).first, 0x0140);  // COMMAND_IMPULSE
}

// An impulse restarts a stopped door, so a frame reporting the stop and the target crossing at once
// must be read as "already stopped" rather than "still opening".
TEST(HoermannHcpPosition, StopReportedWithTheCrossingSendsNoImpulse) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.set_position(0.5f);
  EXPECT_EQ(poll_command(door).first, 0x0110);

  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0014, 0x0100}));
  ASSERT_EQ(door.get_door_state(), DoorState::OPENING);

  // Same frame: position 0.6 (past the target) and state 0x20 -> the door has reached its open end stop.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x2000}));
  ASSERT_EQ(door.get_door_state(), DoorState::OPEN);
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// A target the door never reaches is dropped once it comes to rest, so a later move is not cut short.
TEST(HoermannHcpPosition, TargetIsDroppedWhenTheDoorStopsShort) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.set_position(0.5f);
  EXPECT_EQ(poll_command(door).first, 0x0110);

  // The door is stopped at 0.3 by a wall button, short of the requested 0.5.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0014, 0x0100}));
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0000}));
  ASSERT_EQ(door.get_door_state(), DoorState::STOPPED);

  // A later manual open must run freely instead of being stopped at the abandoned target.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0050, 0x0100}));
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// A new position while the door moves only stops it.
TEST(HoermannHcpPosition, NewPositionWhileMovingStopsTheDoor) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0200}));
  ASSERT_EQ(door.get_door_state(), DoorState::CLOSING);

  EXPECT_TRUE(door.set_position(0.5f));
  EXPECT_EQ(poll_command(door).first, 0x0140);  // COMMAND_IMPULSE
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0100}));
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// A door that never starts has to lose the target, otherwise it would cut a later move short.
// A target outlives a start reported late, as long as it comes within the start window.
TEST(HoermannHcpPosition, TargetSurvivesALateStart) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0000}));
  ASSERT_TRUE(door.set_position(0.5f));
  EXPECT_EQ(poll_command(door).first, 0x0110);  // COMMAND_OPEN
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  connect_controller(door);
  door.update();
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003E, 0x0100}));
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0100}));
  EXPECT_EQ(poll_command(door).first, 0x0140);  // COMMAND_IMPULSE
}

TEST(HoermannHcpPosition, TargetIsDroppedWhenTheDoorNeverStarts) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 200;
  door.start_window_ms_ = 200;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0000}));
  ASSERT_EQ(door.get_door_state(), DoorState::STOPPED);

  door.set_position(0.5f);
  EXPECT_EQ(poll_command(door).first, 0x0110);

  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  // The door ignored the command. Its broadcast keeps the connection alive, so the target is the only thing
  // that may expire here.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0000}));
  door.update();
  ASSERT_TRUE(door.is_valid());

  // A later manual open must run freely instead of being stopped at the abandoned target.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003E, 0x0100}));
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x006E, 0x0100}));
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

}  // namespace esphome::hoermann_hcp::testing
