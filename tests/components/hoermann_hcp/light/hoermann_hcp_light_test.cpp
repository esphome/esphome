#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "esphome/components/hoermann_hcp/light/hoermann_hcp_light.h"

#include "../common.h"

namespace esphome::hoermann_hcp::testing {

namespace {

// Counts how often the platform is asked to write, so a publish that re-triggers itself becomes visible.
class CountingHoermannHcpLight : public HoermannHcpLight {
 public:
  using HoermannHcpLight::HoermannHcpLight;

  void write_state(light::LightState *state) override {
    this->writes++;
    HoermannHcpLight::write_state(state);
  }

  int writes{0};
};

// Drives the platform against a real LightState. ALWAYS_OFF keeps setup() clear of preferences.
struct LightFixture {
  TestableHoermannHcp door;
  CountingHoermannHcpLight output{&door};
  light::LightState state{&output};

  explicit LightFixture(light::LightRestoreMode restore_mode = light::LIGHT_ALWAYS_OFF) {
    this->state.set_restore_mode(restore_mode);
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
  ASSERT_TRUE(fixture.door.is_light_toggle_pending_());

  // A door movement sets changed_, firing the state callback while the toggle is still queued.
  fixture.report_broadcast(make_registers({0x0000, 0x0064, 0x0100}));

  EXPECT_TRUE(fixture.door.is_light_toggle_pending_());
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
  ASSERT_TRUE(fixture.door.is_light_toggle_pending_());

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
  ASSERT_FALSE(fixture.door.is_light_toggle_pending_());
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
  ASSERT_TRUE(fixture.door.is_light_toggle_pending_());
  EXPECT_TRUE(fixture.entity_on());

  // The controller keeps broadcasting but never fetches the command, so the connection stays up.
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  fixture.pump();

  EXPECT_FALSE(fixture.door.is_light_toggle_pending_());
  EXPECT_FALSE(fixture.entity_on());
}

// Losing the bus controller discards the queued toggle too, so the entity must not keep showing it once the
// controller is back and still reporting the lamp unchanged.
TEST(HoermannHcpLightPlatformTest, ToggleLostWithTheConnectionReturnsTheEntityToTheLamp) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.is_light_toggle_pending_());

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.pump();  // the connection times out and the command goes with it
  ASSERT_FALSE(fixture.door.is_valid());

  connect_controller(fixture.door);
  fixture.report_lamp(false);
  EXPECT_FALSE(fixture.entity_on());
}

// The lamp can be switched at the door while the bus is quiet, so what was read before an outage must not
// decide whether a toggle is needed after it.
TEST(HoermannHcpLightPlatformTest, LampIsNotTrustedAcrossAConnectionLoss) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();
  fixture.report_lamp(true);
  ASSERT_TRUE(fixture.entity_on());

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.pump();
  ASSERT_FALSE(fixture.door.is_valid());

  // Back on the bus, but nothing has said what the lamp is doing yet.
  connect_controller(fixture.door);
  fixture.pump();
  ASSERT_TRUE(fixture.door.is_valid());
  ASSERT_FALSE(fixture.door.is_light_known());

  fixture.command(false);
  auto [idle, idle_2] = poll_command(fixture.door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
}

// A door that never reports the lamp leaves the entity unable to do anything, so it must not look healthy.
TEST(HoermannHcpLightPlatformTest, UnreportedLampIsFlaggedOnTheEntity) {
  LightFixture fixture;
  connect_controller(fixture.door);
  fixture.pump();
  ASSERT_TRUE(fixture.door.is_valid());
  EXPECT_TRUE(fixture.output.status_has_warning());

  fixture.report_lamp(false);
  EXPECT_FALSE(fixture.output.status_has_warning());
}

// Two outstanding toggles leave the lamp where it started, so a third tap has to be judged against that and
// withdraw the one still waiting rather than deciding nothing is needed.
TEST(HoermannHcpLightPlatformTest, ThirdTapWithTwoTogglesOutstandingIsHonoured) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  poll_command(fixture.door);
  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  poll_command(fixture.door);  // the first toggle is released but not reported back
  fixture.command(false);
  ASSERT_TRUE(fixture.door.is_light_toggle_pending_());
  ASSERT_EQ(fixture.door.light_toggles_in_flight_, 2);

  // Two toggles cancel out, so asking for on again means withdrawing the second one.
  fixture.command(true);
  EXPECT_FALSE(fixture.door.is_light_toggle_pending_());
  EXPECT_EQ(fixture.door.light_toggles_in_flight_, 1);
  EXPECT_TRUE(fixture.entity_on());
}

// The boot replay is the first write and nothing else, so a real command arriving before the hub's next poll
// must not be mistaken for it and swallowed.
TEST(HoermannHcpLightPlatformTest, CommandBeforeTheFirstPollIsNotMistakenForTheBootReplay) {
  LightFixture fixture;
  connect_controller(fixture.door);
  fixture.settle();  // the boot replay lands here, while the lamp is still unknown

  // The first status broadcast arrives, but the hub has not polled yet, so no callback has fired.
  fixture.door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(fixture.door.is_light_known());

  fixture.command(true);
  auto [pressed, pressed_2] = poll_command(fixture.door);
  EXPECT_EQ(pressed, 0x0100);  // COMMAND_TOGGLE_LAMP
  EXPECT_EQ(pressed_2, 0x0200);
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
  ASSERT_FALSE(fixture.door.is_light_toggle_pending_());
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

// A door that takes the key press but never actually switches the lamp must not leave the entity showing the
// request for ever; the wait has to end so the entity can settle back on what the door reports.
TEST(HoermannHcpLightPlatformTest, ToggleTheDoorIgnoresStopsBeingWaitedFor) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  fixture.command(true);
  consume_command(fixture.door);  // the door takes press and release, then does nothing
  ASSERT_FALSE(fixture.door.is_light_toggle_pending_());
  EXPECT_TRUE(fixture.entity_on());

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.report_lamp(false);  // the lamp is still off, and keeps saying so
  EXPECT_FALSE(fixture.entity_on());
}

// A resting door's first broadcast changes nothing except the lamp finally being reported, so unless that
// counts as a change the light never hears about it and swallows the first command.
TEST(HoermannHcpLightPlatformTest, FirstLampReportReachesTheEntity) {
  LightFixture fixture;
  // A command poll connects the controller without saying anything about the lamp.
  connect_controller(fixture.door);
  fixture.pump();
  ASSERT_FALSE(fixture.door.is_light_known());

  // Closed, at rest, lamp off: every field matches the defaults the hub started with.
  fixture.report_broadcast(make_registers({0x0000, 0x0000, 0x4000, 0x0000, 0x0000, 0x0000, 0x0000}));
  ASSERT_TRUE(fixture.door.is_light_known());

  fixture.command(true);
  auto [pressed, pressed_2] = poll_command(fixture.door);
  EXPECT_EQ(pressed, 0x0100);  // COMMAND_TOGGLE_LAMP
  EXPECT_EQ(pressed_2, 0x0200);
}

// A lost connection means the door can travel unwatched, so a target left armed would stop it long afterwards.
// Which command happened to be in the slot must not change that.
TEST(HoermannHcpLightTest, ConnectionLossWithALampTogglePendingClearsTheTarget) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  // Position 60/200 = 0.3 while opening, so a 0.5 target is armed and under way.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);
  ASSERT_TRUE(door.toggle_light());

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  door.update();
  ASSERT_FALSE(door.is_valid());

  // Back on the bus and travelling past where the target was: nothing should stop the door now.
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  auto [pressed, pressed_2] = poll_command(door);
  EXPECT_EQ(pressed, 0x0000);
  EXPECT_EQ(pressed_2, 0x0000);
}

// Withdrawing a later toggle must not take the deadline of the one already on the wire with it, or a door
// that never reports the lamp would leave the entity waiting for ever.
TEST(HoermannHcpLightPlatformTest, WithdrawingALaterToggleKeepsTheWatchdogArmed) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  fixture.command(true);
  consume_command(fixture.door);  // the first toggle is released but never reported back
  fixture.command(false);
  ASSERT_EQ(fixture.door.light_toggles_in_flight_, 2);
  fixture.command(true);  // withdraws the second, leaving the first outstanding
  ASSERT_EQ(fixture.door.light_toggles_in_flight_, 1);

  // The door still says nothing about the lamp, so the wait has to time out on its own.
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.report_lamp(false);
  EXPECT_EQ(fixture.door.light_toggles_in_flight_, 0);
  EXPECT_FALSE(fixture.entity_on());
}

// A request refused while the lamp is unknown must leave the entity idle. Republishing unconditionally would
// re-enter write_state() on every loop, so the platform would never stop asking to be written.
TEST(HoermannHcpLightPlatformTest, RefusedRequestLeavesTheEntityIdle) {
  LightFixture fixture;
  connect_controller(fixture.door);
  fixture.settle();
  ASSERT_FALSE(fixture.door.is_light_known());

  // The lamp is unknown and the entity already shows off, so asking for off cannot be serviced or displayed.
  fixture.command(false);
  const int settled_writes = fixture.output.writes;
  fixture.settle();
  EXPECT_EQ(fixture.output.writes, settled_writes);
}

// A door that acts on the key press and reports the lamp before the release is even fetched leaves nothing
// outstanding. Arming the watchdog on that release anyway would leave it firing on every poll and abandoning
// the next toggle the moment it is queued.
TEST(HoermannHcpLightTest, ReleaseWithNothingOutstandingLeavesTheWatchdogDisarmed) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.toggle_light());
  poll_command(door);  // the door is shown the key press

  // The door acts on it and reports the lamp straight away, which settles the count.
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));
  ASSERT_EQ(door.light_toggles_in_flight_, 0);

  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  poll_command(door);  // the release, with nothing left to wait for
  EXPECT_EQ(door.light_toggle_released_at_, 0u);
}

// A restore mode that boots the entity on replays a lit state the door has never confirmed, so it has to be
// adopted back to what is known rather than turned into a command.
TEST(HoermannHcpLightPlatformTest, RestoredOnStateIsAdoptedNotCommanded) {
  LightFixture fixture{light::LIGHT_ALWAYS_ON};
  connect_controller(fixture.door);
  fixture.settle();

  auto [idle, idle_2] = poll_command(fixture.door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
  EXPECT_FALSE(fixture.entity_on());
}

// A reversing press before the toggle is fetched cancels it, so the lamp never moves.
TEST(HoermannHcpLightPlatformTest, ReversingPressCancelsTheQueuedToggle) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.is_light_toggle_pending_());

  fixture.command(false);
  EXPECT_FALSE(fixture.door.is_light_toggle_pending_());
  EXPECT_FALSE(fixture.entity_on());

  // Nothing is left for the controller to fetch, so the lamp stays off as asked.
  auto [pressed, pressed_2] = poll_command(fixture.door);
  EXPECT_EQ(pressed, 0x0000);
  EXPECT_EQ(pressed_2, 0x0000);
}

// A lamp switched at the door itself is not one of our toggles landing, so a toggle the door has not even
// been shown has to keep counting.
TEST(HoermannHcpLightTest, DoorSideLampChangeLeavesAnUnsentToggleCounted) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.toggle_light());

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));

  EXPECT_EQ(door.light_toggles_in_flight_, 1);
  // The toggle still in the slot will invert what the door just reported.
  EXPECT_FALSE(door.is_light_heading_on());
}

// Once the toggles left over are all still waiting in the slot, nothing the door has seen is outstanding,
// so the wait has to end rather than time out against toggles the door was never shown.
TEST(HoermannHcpLightTest, SettlingTheLastSentToggleEndsTheWait) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.toggle_light());
  consume_command(door);             // shown to the door, so the wait for a lamp report starts
  ASSERT_TRUE(door.toggle_light());  // queued behind it, never shown
  ASSERT_NE(door.light_toggle_released_at_, 0u);

  // The door reports the lamp change the first toggle caused, leaving only the unsent one.
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));

  ASSERT_EQ(door.light_toggles_in_flight_, 1);
  EXPECT_EQ(door.light_toggle_released_at_, 0u);
}

// The watchdog gives up on the toggles the door was shown, but one still waiting in the command slot is
// going to fire, so it keeps counting.
TEST(HoermannHcpLightTest, WatchdogKeepsAToggleTheDoorHasNotSeen) {
  TestableHoermannHcp door;
  // Wide enough that the toggle queued after the sleep cannot expire before update() runs.
  door.connection_timeout_ms_ = 200;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.toggle_light());
  consume_command(door);  // shown to the door, which then says nothing about the lamp

  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  // Queued just now, so only the wait for the first toggle is overdue.
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.toggle_light());
  door.update();

  EXPECT_EQ(door.light_toggles_in_flight_, 1);
  EXPECT_TRUE(door.is_light_toggle_pending_());
  EXPECT_TRUE(door.is_light_heading_on());
}

// Only the parity of the outstanding count says where the lamp is heading, so the count must not run away.
TEST(HoermannHcpLightTest, TogglesAreRefusedOnceTooManyAreOutstanding) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));

  // The door takes every key press but never reports the lamp, so nothing is ever confirmed.
  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(door.toggle_light());
    consume_command(door);
  }

  EXPECT_FALSE(door.toggle_light());
  EXPECT_EQ(door.light_toggles_in_flight_, 4);
}

// A controller that stops carrying the lamp register leaves nothing refreshing it, so the entity has to flag
// itself rather than command against what was read before.
TEST(HoermannHcpLightPlatformTest, BroadcastWithoutTheLampRegisterMarksItUnknown) {
  LightFixture fixture;
  fixture.bring_up();
  ASSERT_TRUE(fixture.door.is_light_known());

  fixture.report_broadcast(make_registers({0x0000, 0x0000, 0x4000}));

  EXPECT_FALSE(fixture.door.is_light_known());
  EXPECT_TRUE(fixture.output.status_has_warning());
}

// A publish of ours only reaches write_state() a loop pass later. If the lamp changed at the door in that
// gap, the write still carries the old value and must not be taken for a request to invert the lamp.
TEST(HoermannHcpLightPlatformTest, PublishOvertakenByTheLampIsNotARequest) {
  LightFixture fixture;
  fixture.bring_up();
  // A door command holds the only command slot, so the request below is refused and the lamp published back.
  ASSERT_TRUE(fixture.door.open_door());

  auto call = fixture.state.make_call();
  call.set_state(true);
  call.perform();
  fixture.state.loop();  // the refusal happens here and schedules the publish for a later pass

  // The slot frees up and the lamp is switched on at the door before that publish arrives.
  consume_command(fixture.door);
  fixture.door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));
  fixture.settle();

  EXPECT_EQ(fixture.door.light_toggles_in_flight_, 0);
  EXPECT_TRUE(fixture.entity_on());
}

}  // namespace esphome::hoermann_hcp::testing
