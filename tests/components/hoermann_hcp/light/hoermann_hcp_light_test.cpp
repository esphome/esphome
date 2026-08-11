#include <gtest/gtest.h>

#include <chrono>
#include <thread>
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
void connect_controller(HoermannHcp &door) { door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000})); }

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

// Releases the key press immediately, so a poll after any elapsed time returns the released values.
class TestableHoermannHcp : public HoermannHcp {
 public:
  TestableHoermannHcp() { this->key_press_delay_ms_ = 0; }

  using HoermannHcp::connection_timeout_ms_;
};

// Enough elapsed time for a zero-length key press to count as held; millis() has 1 ms resolution.
constexpr auto KEY_PRESS_ELAPSED = std::chrono::milliseconds(2);

// Presents and then releases the queued command, leaving the slot free.
void consume_command(HoermannHcp &door) {
  poll_command(door);
  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  poll_command(door);
}

// Drives the platform against a real LightState. ALWAYS_OFF keeps setup() clear of preferences.
struct LightFixture {
  TestableHoermannHcp door;
  HoermannHcpLight output{&door};
  light::LightState state{&output};

  LightFixture() {
    this->state.set_restore_mode(light::LIGHT_ALWAYS_OFF);
    this->output.setup();
    // setup() queues the restored state for write_state(); the first settle() below delivers it, which is the
    // boot ordering tests need to be able to place around the bus controller coming up.
    this->state.setup();
  }

  // Brings the bus controller up and lets the platform read the lamp once, which is what a device does before
  // any user command can arrive.
  void bring_up() {
    connect_controller(this->door);
    this->report_lamp(false);
  }

  // Issues a command the way Home Assistant would, then lets the state machine settle.
  void command(bool on) {
    auto call = this->state.make_call();
    call.set_state(on);
    call.perform();
    this->settle();
  }

  // Delivers a status broadcast and runs the hub's notification pass.
  void report_broadcast(const RegisterValues &registers) {
    this->door.on_write_registers(BROADCAST_REG, registers);
    this->pump();
  }

  void report_lamp(bool on) { this->report_broadcast(lamp_broadcast(on ? 0x0010 : 0x0000)); }

  // Runs the hub's notification pass and lets the resulting publishes settle.
  void pump() {
    this->door.update();
    this->settle();
  }

  void settle() {
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

// The lamp command is the only one that drives the second command register, on both halves of the press.
TEST(HoermannHcpLightTest, LampCommandUsesTheSecondRegister) {
  TestableHoermannHcp door;
  connect_controller(door);
  ASSERT_FALSE(door.is_light_on());
  ASSERT_TRUE(door.toggle_light());

  auto [pressed, pressed_2] = poll_command(door);
  EXPECT_EQ(pressed, 0x0100);
  EXPECT_EQ(pressed_2, 0x0200);

  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  auto [released, released_2] = poll_command(door);
  EXPECT_EQ(released, 0x0800);
  EXPECT_EQ(released_2, 0x0200);

  // The command is spent, so the next poll carries nothing.
  auto [idle, idle_2] = poll_command(door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
}

// Toggling the lamp must not disturb a cover position the door is still travelling to.
TEST(HoermannHcpLightTest, LampToggleKeepsTheCoverTarget) {
  TestableHoermannHcp door;
  connect_controller(door);
  // Position 60/200 = 0.3 while opening, so a 0.5 target is armed and under way.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);

  ASSERT_TRUE(door.toggle_light());
  consume_command(door);

  // Past the target: the door still has to be stopped despite the lamp command in between.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  auto [pressed, pressed_2] = poll_command(door);
  EXPECT_EQ(pressed, 0x0240);  // COMMAND_IMPULSE
  EXPECT_EQ(pressed_2, 0x0000);
}

// A lamp toggle occupies the single command slot, so a target stop falling due while it waits to be fetched
// has to wait too. The target stays armed and the stop goes out on the next position report, which costs the
// door a little overshoot but never loses the stop.
TEST(HoermannHcpLightTest, LampToggleDelaysButDoesNotLoseTheTargetStop) {
  TestableHoermannHcp door;
  connect_controller(door);
  // Position 60/200 = 0.3 while opening, so a 0.5 target is armed and under way.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);

  ASSERT_TRUE(door.toggle_light());
  // The door passes the target while the lamp toggle still holds the slot, so the lamp goes out first.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  auto [pressed, pressed_2] = poll_command(door);
  EXPECT_EQ(pressed, 0x0100);
  EXPECT_EQ(pressed_2, 0x0200);
  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  poll_command(door);

  // The target survived the refusal, so the next position report still stops the door.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0079, 0x0100}));
  auto [stop, stop_2] = poll_command(door);
  EXPECT_EQ(stop, 0x0240);  // COMMAND_IMPULSE
  EXPECT_EQ(stop_2, 0x0000);
}

// The target's start deadline is its own, so toggling the lamp cannot keep a stale target alive.
TEST(HoermannHcpLightTest, LampToggleDoesNotExtendTheTargetWatchdog) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  // The door is closing, so an opening target is armed but not yet under way.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0200}));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  ASSERT_TRUE(door.toggle_light());
  consume_command(door);
  door.update();

  // The target expired on its own schedule, so a later opening move runs freely.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0050, 0x0100}));
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  auto [pressed, pressed_2] = poll_command(door);
  EXPECT_EQ(pressed, 0x0000);
  EXPECT_EQ(pressed_2, 0x0000);
}

// Without a bus controller the command cannot be delivered, and the caller is told.
TEST(HoermannHcpLightTest, LampCommandIsRefusedWhileDisconnected) {
  HoermannHcp door;
  EXPECT_FALSE(door.toggle_light());
}

// Switching the entity on sends one toggle, and the door's own report does not send a second.
TEST(HoermannHcpLightPlatformTest, CommandTogglesOnceAndSettles) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  auto [pressed, pressed_2] = poll_command(fixture.door);
  EXPECT_EQ(pressed, 0x0100);
  EXPECT_EQ(pressed_2, 0x0200);
  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  poll_command(fixture.door);  // release, clearing the slot

  // The lamp is now on, and the resulting broadcast must not queue another toggle.
  fixture.report_lamp(true);
  EXPECT_TRUE(fixture.entity_on());
  auto [idle, idle_2] = poll_command(fixture.door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
}

// A broadcast arriving while a toggle is queued must not reconcile against the not-yet-inverted lamp, which
// would cancel the user's own command.
TEST(HoermannHcpLightPlatformTest, BroadcastDuringPendingToggleKeepsTheCommand) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.is_light_toggle_pending());

  // A door movement sets changed_, firing the state callback while the toggle is still queued.
  fixture.report_broadcast(make_registers({0x0000, 0x0064, 0x0100}));

  EXPECT_TRUE(fixture.door.is_light_toggle_pending());
  EXPECT_TRUE(fixture.entity_on());
}

// A lamp switched on at the door itself has to reach the entity.
TEST(HoermannHcpLightPlatformTest, DoorDrivenChangeReachesTheEntity) {
  LightFixture fixture;
  fixture.bring_up();
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

// A reversing press once the toggle is already on the wire cannot stop it, so the entity has to end up
// showing the lamp rather than the request that was refused.
TEST(HoermannHcpLightPlatformTest, RefusedPressAfterFetchShowsWhereTheLampIsHeading) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  poll_command(fixture.door);  // the controller fetches the press, so it can no longer be cancelled
  ASSERT_TRUE(fixture.door.is_light_toggle_pending());

  fixture.command(false);
  EXPECT_TRUE(fixture.entity_on());

  // A door movement while the refused toggle is still on the wire must not pull the entity back either.
  fixture.report_broadcast(make_registers({0x0000, 0x0064, 0x0100}));
  EXPECT_TRUE(fixture.entity_on());

  // The toggle lands and the door confirms it; the entity must already agree.
  fixture.report_lamp(true);
  EXPECT_TRUE(fixture.entity_on());
}

// The lamp is only reported some time after the key press is released, so an unrelated door broadcast in
// that gap must not publish the state the lamp is about to leave.
TEST(HoermannHcpLightPlatformTest, DoorMovementDoesNotFlipTheEntityBeforeTheLampReports) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  poll_command(fixture.door);
  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  poll_command(fixture.door);  // release, so nothing is pending any more
  ASSERT_FALSE(fixture.door.is_light_toggle_pending());
  ASSERT_FALSE(fixture.door.is_light_on());  // the lamp has still not been reported

  fixture.report_broadcast(make_registers({0x0000, 0x0064, 0x0100}));

  EXPECT_TRUE(fixture.entity_on());
}

// A toggle the controller never fetches is eventually dropped, and nothing else will ever report the lamp
// moving, so the entity has to be brought back to what the lamp actually is.
TEST(HoermannHcpLightPlatformTest, DroppedToggleReturnsTheEntityToTheLamp) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.is_light_toggle_pending());
  EXPECT_TRUE(fixture.entity_on());

  // The controller keeps broadcasting but never fetches the command, so the connection stays up.
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  fixture.pump();

  EXPECT_FALSE(fixture.door.is_light_toggle_pending());
  EXPECT_FALSE(fixture.entity_on());
}

// Losing the bus controller discards the queued toggle too, so the entity must not keep showing it once the
// controller is back and still reporting the lamp unchanged.
TEST(HoermannHcpLightPlatformTest, ToggleLostWithTheConnectionReturnsTheEntityToTheLamp) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.is_light_toggle_pending());

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.pump();  // the connection times out and the command goes with it
  ASSERT_FALSE(fixture.door.is_valid());

  connect_controller(fixture.door);
  fixture.pump();
  EXPECT_FALSE(fixture.entity_on());
}

// On boot the restored state is replayed through write_state() before the lamp has ever been read. A lamp
// that is already on must not be switched off by that replay.
TEST(HoermannHcpLightPlatformTest, RestoredStateOnBootDoesNotCommandTheLamp) {
  LightFixture fixture;
  // The controller is already up and reporting the lamp lit before the entity's first loop.
  connect_controller(fixture.door);
  fixture.door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));
  ASSERT_TRUE(fixture.door.is_light_on());

  fixture.settle();
  auto [idle, idle_2] = poll_command(fixture.door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
  // Once the platform has read the lamp the entity follows it, still without commanding anything.
  fixture.pump();
  EXPECT_TRUE(fixture.entity_on());
}

// Bus traffic makes the connection valid without saying anything about the lamp, so a request arriving before
// the first status broadcast must not be judged against a lamp state that was never read.
TEST(HoermannHcpLightPlatformTest, RequestBeforeTheLampIsReportedDoesNotCommandTheLamp) {
  LightFixture fixture;
  // The controller polls for commands, which is enough to connect but carries no lamp register.
  connect_controller(fixture.door);
  fixture.pump();
  ASSERT_TRUE(fixture.door.is_valid());
  ASSERT_FALSE(fixture.door.is_light_known());

  fixture.command(true);
  auto [idle, idle_2] = poll_command(fixture.door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
  EXPECT_FALSE(fixture.entity_on());
}

// A toggle that has been released onto the wire is no longer pending, but the lamp has not reported it yet.
// A reversing request in that window is a real request and has to be sent, not swallowed.
TEST(HoermannHcpLightPlatformTest, ReversingRequestAfterReleaseQueuesASecondToggle) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  poll_command(fixture.door);
  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  poll_command(fixture.door);  // released, so nothing is pending and the lamp is still unreported
  ASSERT_FALSE(fixture.door.is_light_toggle_pending());
  ASSERT_FALSE(fixture.door.is_light_on());

  fixture.command(false);
  auto [pressed, pressed_2] = poll_command(fixture.door);
  EXPECT_EQ(pressed, 0x0100);  // COMMAND_TOGGLE_LAMP
  EXPECT_EQ(pressed_2, 0x0200);
  EXPECT_FALSE(fixture.entity_on());

  // The first toggle lands and is reported, but the entity is already heading for off.
  fixture.report_lamp(true);
  EXPECT_FALSE(fixture.entity_on());

  // The second toggle lands too, and the lamp finally agrees with the request.
  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  poll_command(fixture.door);
  fixture.report_lamp(false);
  EXPECT_FALSE(fixture.entity_on());
}

// A refusal that has no toggle on the wire leaves nothing outstanding, so it must not latch the entity
// against the next lamp change the door reports.
TEST(HoermannHcpLightPlatformTest, RefusalWithoutAToggleStillFollowsTheLamp) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.pump();
  ASSERT_FALSE(fixture.door.is_valid());

  // Refused because the bus is down, so no toggle is heading for the lamp.
  fixture.command(true);
  EXPECT_FALSE(fixture.entity_on());

  // The controller returns and reports the lamp switched on at the door itself.
  connect_controller(fixture.door);
  fixture.report_lamp(true);
  EXPECT_TRUE(fixture.entity_on());
}

// A lamp toggle carries no target, so dropping it unfetched must leave the cover's target alone.
TEST(HoermannHcpLightTest, DroppedLampToggleKeepsTheCoverTarget) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  // Position 60/200 = 0.3 while opening, so a 0.5 target is armed and under way.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);

  // The controller keeps broadcasting but stops fetching, so the lamp toggle expires on its own.
  ASSERT_TRUE(door.toggle_light());
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0050, 0x0100}));
  door.update();

  // The target survived the lamp toggle being dropped, so the door is still stopped on the way.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  auto [pressed, pressed_2] = poll_command(door);
  EXPECT_EQ(pressed, 0x0240);  // COMMAND_IMPULSE
  EXPECT_EQ(pressed_2, 0x0000);
}

// A reversing press before the toggle is fetched cancels it, so the lamp never moves.
TEST(HoermannHcpLightPlatformTest, ReversingPressCancelsTheQueuedToggle) {
  LightFixture fixture;
  fixture.bring_up();

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
