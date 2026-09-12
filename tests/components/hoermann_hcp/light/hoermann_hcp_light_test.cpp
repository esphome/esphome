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

// The lamp is asked for a state rather than toggled, so each direction has its own value, sent once.
TEST(HoermannHcpLightTest, LampCommandsNameTheState) {
  HoermannHcp door;
  connect_controller(door);

  ASSERT_TRUE(door.set_light(true));
  auto [on, on_2] = poll_command(door);
  EXPECT_EQ(on, 0x0880);
  EXPECT_EQ(on_2, 0x0000);
  // The command is spent, so the next poll carries nothing.
  auto [idle, idle_2] = poll_command(door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);

  ASSERT_TRUE(door.set_light(false));
  auto [off, off_2] = poll_command(door);
  EXPECT_EQ(off, 0x0800);
  EXPECT_EQ(off_2, 0x0100);
}

// A lamp request must not disturb a cover position the door is still travelling to.
TEST(HoermannHcpLightTest, LampRequestKeepsTheCoverTarget) {
  HoermannHcp door;
  connect_controller(door);
  // Position 60/200 = 0.3 while opening, so a 0.5 target is armed and under way.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);

  ASSERT_TRUE(door.set_light(true));
  consume_command(door);

  // Past the target: the door still has to be stopped despite the lamp command in between.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  auto [value, value_2] = poll_command(door);
  EXPECT_EQ(value, 0x0140);  // COMMAND_IMPULSE
  EXPECT_EQ(value_2, 0x0000);
}

// A lamp request occupies the single command slot, so a target stop falling due while it waits to be fetched
// has to wait too. The target stays armed and the stop goes out on the next position report, which costs the
// door a little overshoot but never loses the stop.
TEST(HoermannHcpLightTest, LampRequestDelaysButDoesNotLoseTheTargetStop) {
  HoermannHcp door;
  connect_controller(door);
  // Position 60/200 = 0.3 while opening, so a 0.5 target is armed and under way.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);

  ASSERT_TRUE(door.set_light(true));
  // The door passes the target while the lamp request still holds the slot, so the lamp goes out first.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  auto [value, value_2] = poll_command(door);
  EXPECT_EQ(value, 0x0880);
  EXPECT_EQ(value_2, 0x0000);

  // The target survived the refusal, so the next position report still stops the door.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0079, 0x0100}));
  auto [stop, stop_2] = poll_command(door);
  EXPECT_EQ(stop, 0x0140);  // COMMAND_IMPULSE
  EXPECT_EQ(stop_2, 0x0000);
}

// The target's start deadline is its own, so a lamp request cannot keep a stale target alive.
TEST(HoermannHcpLightTest, LampRequestDoesNotExtendTheTargetWatchdog) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  // The door is closing, so an opening target is armed but not yet under way.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0200}));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  ASSERT_TRUE(door.set_light(true));
  consume_command(door);
  door.update();

  // The target expired on its own schedule, so a later opening move runs freely.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0050, 0x0100}));
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  auto [value, value_2] = poll_command(door);
  EXPECT_EQ(value, 0x0000);
  EXPECT_EQ(value_2, 0x0000);
}

// Without a bus controller the command cannot be delivered, and the caller is told.
TEST(HoermannHcpLightTest, LampCommandIsRefusedWhileDisconnected) {
  HoermannHcp door;
  EXPECT_FALSE(door.set_light(true));
}

// A request the controller has not fetched is replaced by a newer one asking for something else, so the door
// only ever sees what is wanted now.
TEST(HoermannHcpLightTest, NewerRequestReplacesTheOneStillQueued) {
  HoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.set_light(false));  // the hub does not judge a request against the lamp; the platform does

  ASSERT_TRUE(door.set_light(true));
  EXPECT_TRUE(door.is_light_heading_on());

  auto [value, value_2] = poll_command(door);
  EXPECT_EQ(value, 0x0880);
  EXPECT_EQ(value_2, 0x0000);
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// A request that only takes back one still waiting is withdrawn, so the door sees neither.
TEST(HoermannHcpLightTest, ReversingRequestBeforeFetchWithdrawsIt) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.set_light(true));

  ASSERT_TRUE(door.set_light(false));
  EXPECT_FALSE(door.light_request_pending_);
  EXPECT_FALSE(door.is_light_heading_on());
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// The lamp reported in the requested state settles the request, whoever switched it. A request still in the
// slot goes out regardless: it names a state, so it changes nothing.
TEST(HoermannHcpLightTest, LampReportedAsRequestedSettlesTheRequest) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.set_light(true));

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));
  EXPECT_FALSE(door.light_request_pending_);
  EXPECT_TRUE(door.is_light_heading_on());
  EXPECT_EQ(poll_command(door).first, 0x0880);
}

// The lamp is reported a moment after the door acted, so a broadcast still carrying the old state is the one
// from before, not a refusal.
TEST(HoermannHcpLightTest, LampStillReportedUnchangedKeepsTheRequest) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.set_light(true));
  consume_command(door);

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  EXPECT_TRUE(door.light_request_pending_);
  EXPECT_TRUE(door.is_light_heading_on());

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));
  EXPECT_FALSE(door.light_request_pending_);
  EXPECT_EQ(door.light_request_sent_at_, 0u);
}

// Switching the entity on sends one command, and the door's own report does not send a second.
TEST(HoermannHcpLightPlatformTest, CommandIsSentOnceAndSettles) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  auto [value, value_2] = poll_command(fixture.door);
  EXPECT_EQ(value, 0x0880);
  EXPECT_EQ(value_2, 0x0000);

  // The lamp is now on, and the resulting broadcast must not queue another command.
  fixture.report_lamp(true);
  EXPECT_TRUE(fixture.entity_on());
  auto [idle, idle_2] = poll_command(fixture.door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
}

// A broadcast arriving while a request is queued must not reconcile against the lamp as it still is, which
// would cancel the user's own command.
TEST(HoermannHcpLightPlatformTest, BroadcastDuringPendingRequestKeepsTheCommand) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.light_request_pending_);

  // A door movement sets changed_, firing the state callback while the request is still queued.
  fixture.report_broadcast(make_registers({0x0000, 0x0064, 0x0100}));

  EXPECT_TRUE(fixture.door.light_request_pending_);
  EXPECT_TRUE(fixture.entity_on());
  EXPECT_EQ(poll_command(fixture.door).first, 0x0880);
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

// A reversing request once the first is on the wire is a second command, and the entity shows the newer one
// until the lamp agrees with it.
TEST(HoermannHcpLightPlatformTest, ReversingRequestAfterFetchIsSentAsWell) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  poll_command(fixture.door);  // the controller fetches the request

  fixture.command(false);
  auto [value, value_2] = poll_command(fixture.door);
  EXPECT_EQ(value, 0x0800);
  EXPECT_EQ(value_2, 0x0100);
  EXPECT_FALSE(fixture.entity_on());

  // The first request lands and is reported, but the entity is already heading for off.
  fixture.report_lamp(true);
  EXPECT_FALSE(fixture.entity_on());

  // The second lands too, and the lamp finally agrees with the request.
  fixture.report_lamp(false);
  EXPECT_FALSE(fixture.entity_on());
  EXPECT_FALSE(fixture.door.light_request_pending_);
}

// The lamp is only reported some time after the command was fetched, so an unrelated door broadcast in that
// gap must not publish the state the lamp is about to leave.
TEST(HoermannHcpLightPlatformTest, DoorMovementDoesNotFlipTheEntityBeforeTheLampReports) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  poll_command(fixture.door);  // fetched, so the slot is free and the lamp still unreported
  ASSERT_FALSE(fixture.door.is_light_on());

  fixture.report_broadcast(make_registers({0x0000, 0x0064, 0x0100}));

  EXPECT_TRUE(fixture.entity_on());
}

// A request the controller never fetches is eventually dropped, and nothing else will ever report the lamp
// moving, so the entity has to be brought back to what the lamp actually is.
TEST(HoermannHcpLightPlatformTest, DroppedRequestReturnsTheEntityToTheLamp) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.light_request_pending_);
  EXPECT_TRUE(fixture.entity_on());

  // The controller keeps broadcasting but never fetches the command, so the connection stays up.
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  fixture.pump();

  EXPECT_FALSE(fixture.door.light_request_pending_);
  EXPECT_FALSE(fixture.entity_on());
}

// Losing the bus controller discards the queued request too, so the entity must not keep showing it once the
// controller is back and still reporting the lamp unchanged.
TEST(HoermannHcpLightPlatformTest, RequestLostWithTheConnectionReturnsTheEntityToTheLamp) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.light_request_pending_);

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.pump();  // the connection times out and the command goes with it
  ASSERT_FALSE(fixture.door.is_valid());

  connect_controller(fixture.door);
  fixture.report_lamp(false);
  EXPECT_FALSE(fixture.entity_on());
}

// The lamp can be switched at the door while the bus is quiet, so what was read before an outage must not
// decide whether a command is needed after it.
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
  auto [value, value_2] = poll_command(fixture.door);
  EXPECT_EQ(value, 0x0880);  // COMMAND_LIGHT_ON
  EXPECT_EQ(value_2, 0x0000);
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

// A refusal that has no request on the wire leaves nothing outstanding, so it must not latch the entity
// against the next lamp change the door reports.
TEST(HoermannHcpLightPlatformTest, RefusalWithoutARequestStillFollowsTheLamp) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.pump();
  ASSERT_FALSE(fixture.door.is_valid());

  // Refused because the bus is down, so no request is heading for the lamp.
  fixture.command(true);
  EXPECT_FALSE(fixture.entity_on());

  // The controller returns and reports the lamp switched on at the door itself.
  connect_controller(fixture.door);
  fixture.report_lamp(true);
  EXPECT_TRUE(fixture.entity_on());
}

// A lamp request carries no target, so dropping it unfetched must leave the cover's target alone.
TEST(HoermannHcpLightTest, DroppedLampRequestKeepsTheCoverTarget) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  // Position 60/200 = 0.3 while opening, so a 0.5 target is armed and under way.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);

  // The controller keeps broadcasting but stops fetching, so the lamp request expires on its own.
  ASSERT_TRUE(door.set_light(true));
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0050, 0x0100}));
  door.update();

  // The target survived the lamp request being dropped, so the door is still stopped on the way.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  auto [value, value_2] = poll_command(door);
  EXPECT_EQ(value, 0x0140);  // COMMAND_IMPULSE
  EXPECT_EQ(value_2, 0x0000);
}

// A door that takes the command but never actually switches the lamp must not leave the entity showing the
// request for ever; the wait has to end so the entity can settle back on what the door reports.
TEST(HoermannHcpLightPlatformTest, RequestTheDoorIgnoresStopsBeingWaitedFor) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  fixture.command(true);
  consume_command(fixture.door);  // the door takes the command, then does nothing
  ASSERT_NE(fixture.door.light_request_sent_at_, 0u);
  EXPECT_TRUE(fixture.entity_on());

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.report_lamp(false);  // the lamp is still off, and keeps saying so
  EXPECT_FALSE(fixture.door.light_request_pending_);
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
  auto [value, value_2] = poll_command(fixture.door);
  EXPECT_EQ(value, 0x0880);  // COMMAND_LIGHT_ON
  EXPECT_EQ(value_2, 0x0000);
}

// A lost connection means the door can travel unwatched, so a target left armed would stop it long afterwards.
// Which command happened to be in the slot must not change that.
TEST(HoermannHcpLightTest, ConnectionLossWithALampRequestPendingClearsTheTarget) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  // Position 60/200 = 0.3 while opening, so a 0.5 target is armed and under way.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x003C, 0x0100}));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);
  ASSERT_TRUE(door.set_light(true));

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  door.update();
  ASSERT_FALSE(door.is_valid());

  // Back on the bus and travelling past where the target was: nothing should stop the door now.
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0078, 0x0100}));
  auto [value, value_2] = poll_command(door);
  EXPECT_EQ(value, 0x0000);
  EXPECT_EQ(value_2, 0x0000);
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

// A reversing request before the first is fetched withdraws it, so the lamp never moves.
TEST(HoermannHcpLightPlatformTest, ReversingRequestBeforeFetchWithdrawsTheQueuedOne) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.light_request_pending_);

  fixture.command(false);
  EXPECT_FALSE(fixture.door.light_request_pending_);
  EXPECT_FALSE(fixture.entity_on());

  // Nothing is left for the controller to fetch, so the lamp stays off as asked.
  auto [value, value_2] = poll_command(fixture.door);
  EXPECT_EQ(value, 0x0000);
  EXPECT_EQ(value_2, 0x0000);
}

// Asking for the state the lamp already reports is nothing to send.
TEST(HoermannHcpLightPlatformTest, RequestForTheReportedStateSendsNothing) {
  LightFixture fixture;
  fixture.bring_up();
  fixture.report_lamp(true);
  ASSERT_TRUE(fixture.entity_on());

  fixture.command(true);
  EXPECT_EQ(poll_command(fixture.door).first, 0x0000);
  EXPECT_TRUE(fixture.entity_on());
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
// gap, the write still carries the old value and must not be taken for a request to switch the lamp back.
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

  EXPECT_FALSE(fixture.door.light_request_pending_);
  EXPECT_TRUE(fixture.entity_on());
  EXPECT_EQ(poll_command(fixture.door).first, 0x0000);
}

}  // namespace esphome::hoermann_hcp::testing
