#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "esphome/components/hoermann_hcp/light/hoermann_hcp_light.h"

#include "../common.h"

namespace esphome::hoermann_hcp::testing {

namespace {

// A status broadcast with the door at the given position and state that also carries the lamp register.
RegisterValues door_broadcast(uint16_t position, uint16_t state, uint16_t lamp = 0x0000) {
  return make_registers({0x0000, position, state, 0x0000, 0x0000, 0x0000, lamp});
}

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

// Drives the platform against a real LightState. Boots off (no persistence) by default, which
// keeps setup() clear of preferences.
struct LightFixture {
  TestableHoermannHcp door;
  CountingHoermannHcpLight output{&door};
  light::LightState state{&output};

  explicit LightFixture(bool boot_on = false) {
    if (boot_on) {
      this->state.set_state_callback([](light::LightStateRTCState &s, bool /*restored*/) { s.state = true; });
    }
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

// A request is sent as one toggle, named in the second register next to 0x0800 in the first. Nothing more
// goes out until the door reports the lamp, which settles the request.
TEST(HoermannHcpLightTest, RequestSendsOneToggleUntilTheLampReports) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.set_light(true));

  auto [toggle, toggle_2] = poll_command(door);
  EXPECT_EQ(toggle, 0x0800);
  EXPECT_EQ(toggle_2, 0x0200);

  // Asking again while the toggle is on its way sends no second one.
  ASSERT_TRUE(door.set_light(true));
  auto [waiting, waiting_2] = poll_command(door);
  EXPECT_EQ(waiting, 0x0000);
  EXPECT_EQ(waiting_2, 0x0000);

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));
  EXPECT_FALSE(door.light_requested_);
  EXPECT_TRUE(door.is_light_heading_on());
  auto [idle, idle_2] = poll_command(door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
}

// The door can only act on a toggle it has fetched, so the wait for the lamp starts there and not when the
// request is made, and the lamp report that confirms it ends the wait again.
TEST(HoermannHcpLightTest, FetchStartsTheWaitForTheLamp) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.set_light(true));
  EXPECT_EQ(door.light_toggle_sent_at_, 0u);

  poll_command(door);
  EXPECT_NE(door.light_toggle_sent_at_, 0u);

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));
  EXPECT_FALSE(door.light_requested_);
  EXPECT_EQ(door.light_toggle_sent_at_, 0u);
}

// A request the lamp already meets is dropped at once, and so is one taken back before the toggle is fetched.
TEST(HoermannHcpLightTest, RequestTheLampAlreadyMeetsSendsNothing) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));

  ASSERT_TRUE(door.set_light(false));
  EXPECT_FALSE(door.light_requested_);

  ASSERT_TRUE(door.set_light(true));
  ASSERT_TRUE(door.set_light(false));
  EXPECT_FALSE(door.light_requested_);
  EXPECT_FALSE(door.is_light_heading_on());
  auto [idle, idle_2] = poll_command(door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
}

// A request reversed after the toggle was fetched cannot stop it, so once it lands the next fetch toggles back.
TEST(HoermannHcpLightTest, RequestReversedAfterTheFetchTogglesAgainOnceTheFirstLands) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.set_light(true));
  EXPECT_EQ(poll_command(door).first, 0x0800);

  ASSERT_TRUE(door.set_light(false));
  EXPECT_FALSE(door.is_light_heading_on());
  // The first toggle has not been reported yet, so nothing is decided.
  EXPECT_EQ(poll_command(door).first, 0x0000);

  // It lands, away from the request.
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));
  EXPECT_TRUE(door.light_requested_);
  EXPECT_EQ(poll_command(door).first, 0x0800);

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  EXPECT_FALSE(door.light_requested_);
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// A broadcast without the lamp register drops the request, so no toggle is decided against the stale state.
TEST(HoermannHcpLightTest, RequestIsNotSentAgainstAStaleLamp) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.set_light(true));

  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x4000}));
  ASSERT_FALSE(door.is_light_known());

  EXPECT_FALSE(door.light_requested_);
  auto [idle, idle_2] = poll_command(door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
}

// A lamp switched at the door with no request outstanding is followed, never switched back.
TEST(HoermannHcpLightTest, DoorSideLampChangeWithoutARequestSendsNothing) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));
  EXPECT_TRUE(door.is_light_heading_on());
  EXPECT_EQ(poll_command(door).first, 0x0000);

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  EXPECT_FALSE(door.is_light_heading_on());
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// The toggle is decided when it is fetched, so a lamp switched to the requested state at the door before then
// settles the request instead of being toggled away again.
TEST(HoermannHcpLightTest, DoorSideLampChangeToTheRequestBeforeTheFetchSendsNothing) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.set_light(true));

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));

  EXPECT_FALSE(door.light_requested_);
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// A door that takes the toggle but never reports the lamp must not keep the request alive for ever, and giving
// up must not send the toggle again.
TEST(HoermannHcpLightTest, WatchdogGivesUpOnAnUnreportedToggle) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.set_light(true));
  EXPECT_EQ(poll_command(door).first, 0x0800);

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  // The broadcast keeps the connection alive, so only the wait for the lamp is overdue.
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  door.update();
  ASSERT_TRUE(door.is_valid());

  EXPECT_FALSE(door.light_requested_);
  EXPECT_EQ(door.light_toggle_sent_at_, 0u);
  EXPECT_FALSE(door.is_light_heading_on());
  EXPECT_EQ(poll_command(door).first, 0x0000);
}

// The lamp does not use the command slot: a door command is accepted beside a lamp request and goes first.
TEST(HoermannHcpLightTest, DoorCommandGoesBeforeTheLampToggle) {
  TestableHoermannHcp door;
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  ASSERT_TRUE(door.set_light(true));
  ASSERT_TRUE(door.close_door());

  EXPECT_EQ(poll_command(door).first, 0x0120);  // COMMAND_CLOSE
  auto [toggle, toggle_2] = poll_command(door);
  EXPECT_EQ(toggle, 0x0800);
  EXPECT_EQ(toggle_2, 0x0200);
}

// Switching the lamp must not disturb a cover position the door is still travelling to.
TEST(HoermannHcpLightTest, LampToggleKeepsTheCoverTarget) {
  TestableHoermannHcp door;
  connect_controller(door);
  // Position 60/200 = 0.3 while opening, so a 0.5 target is armed and under way.
  door.on_write_registers(BROADCAST_REG, door_broadcast(0x003C, 0x0100));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);

  ASSERT_TRUE(door.set_light(true));
  EXPECT_EQ(poll_command(door).first, 0x0800);

  // Past the target: the door still has to be stopped despite the lamp toggle in between.
  door.on_write_registers(BROADCAST_REG, door_broadcast(0x0078, 0x0100));
  auto [stop, stop_2] = poll_command(door);
  EXPECT_EQ(stop, 0x0140);  // COMMAND_IMPULSE
  EXPECT_EQ(stop_2, 0x0000);
}

// A target stop falling due while a lamp toggle waits to be fetched goes out first, so the lamp costs the door
// no overshoot.
TEST(HoermannHcpLightTest, LampRequestDoesNotDelayTheTargetStop) {
  TestableHoermannHcp door;
  connect_controller(door);
  // Position 60/200 = 0.3 while opening, so a 0.5 target is armed and under way.
  door.on_write_registers(BROADCAST_REG, door_broadcast(0x003C, 0x0100));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);

  ASSERT_TRUE(door.set_light(true));
  door.on_write_registers(BROADCAST_REG, door_broadcast(0x0078, 0x0100));
  auto [stop, stop_2] = poll_command(door);
  EXPECT_EQ(stop, 0x0140);  // COMMAND_IMPULSE
  EXPECT_EQ(stop_2, 0x0000);
  auto [toggle, toggle_2] = poll_command(door);
  EXPECT_EQ(toggle, 0x0800);
  EXPECT_EQ(toggle_2, 0x0200);
}

// The target's start deadline is its own, so switching the lamp cannot keep a stale target alive.
TEST(HoermannHcpLightTest, LampToggleDoesNotExtendTheTargetWatchdog) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  // The door is closing, so an opening target is armed but not yet under way.
  door.on_write_registers(BROADCAST_REG, door_broadcast(0x003C, 0x0200));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  ASSERT_TRUE(door.set_light(true));
  consume_command(door);
  door.update();

  // The target expired on its own schedule, so a later opening move runs freely.
  door.on_write_registers(BROADCAST_REG, door_broadcast(0x0050, 0x0100));
  door.on_write_registers(BROADCAST_REG, door_broadcast(0x0078, 0x0100));
  auto [idle, idle_2] = poll_command(door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
}

// Giving up on an unreported lamp toggle says nothing about the door, so the cover's target stays.
TEST(HoermannHcpLightTest, LampWatchdogKeepsTheCoverTarget) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  // Position 60/200 = 0.3 while opening, so a 0.5 target is armed and under way.
  door.on_write_registers(BROADCAST_REG, door_broadcast(0x003C, 0x0100));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);

  // The door takes the toggle but never reports the lamp, so the wait for it runs out.
  ASSERT_TRUE(door.set_light(true));
  consume_command(door);
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  door.on_write_registers(BROADCAST_REG, door_broadcast(0x0050, 0x0100));
  door.update();
  ASSERT_FALSE(door.light_requested_);

  // The target survived, so the door is still stopped on the way.
  door.on_write_registers(BROADCAST_REG, door_broadcast(0x0078, 0x0100));
  auto [stop, stop_2] = poll_command(door);
  EXPECT_EQ(stop, 0x0140);  // COMMAND_IMPULSE
  EXPECT_EQ(stop_2, 0x0000);
}

// A lost connection means the door and the lamp can change unwatched, so neither a target nor a lamp request
// may outlive it.
TEST(HoermannHcpLightTest, ConnectionLossClearsTheTargetAndTheLampRequest) {
  TestableHoermannHcp door;
  door.connection_timeout_ms_ = 20;
  connect_controller(door);
  // Position 60/200 = 0.3 while opening, so a 0.5 target is armed and under way.
  door.on_write_registers(BROADCAST_REG, door_broadcast(0x003C, 0x0100));
  ASSERT_TRUE(door.set_position(0.5f));
  consume_command(door);
  ASSERT_TRUE(door.set_light(true));

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  door.update();
  ASSERT_FALSE(door.is_valid());
  EXPECT_FALSE(door.light_requested_);

  // Back on the bus, travelling past where the target was, lamp still off: nothing is sent.
  connect_controller(door);
  door.on_write_registers(BROADCAST_REG, door_broadcast(0x0078, 0x0100));
  auto [idle, idle_2] = poll_command(door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
}

// Without a bus controller the request cannot be delivered, and the caller is told.
TEST(HoermannHcpLightTest, LampRequestIsRefusedWhileDisconnected) {
  HoermannHcp door;
  EXPECT_FALSE(door.set_light(true));
}

// A lamp that has not been reported cannot be switched towards a state, so the request is refused.
TEST(HoermannHcpLightTest, LampRequestIsRefusedUntilTheLampIsReported) {
  TestableHoermannHcp door;
  connect_controller(door);
  EXPECT_FALSE(door.set_light(true));

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  EXPECT_TRUE(door.set_light(true));
}

// Switching the entity on sends one toggle, and the door's own report does not send a second.
TEST(HoermannHcpLightPlatformTest, CommandTogglesOnceAndSettles) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  auto [toggle, toggle_2] = poll_command(fixture.door);
  EXPECT_EQ(toggle, 0x0800);
  EXPECT_EQ(toggle_2, 0x0200);

  // The lamp is now on, and the resulting broadcast must not send another toggle.
  fixture.report_lamp(true);
  EXPECT_TRUE(fixture.entity_on());
  EXPECT_FALSE(fixture.door.light_requested_);
  auto [idle, idle_2] = poll_command(fixture.door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
}

// A broadcast arriving while a request waits to be fetched must not reconcile against the lamp that has not
// moved yet, which would take back the user's own command.
TEST(HoermannHcpLightPlatformTest, BroadcastDuringPendingRequestKeepsTheCommand) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.light_requested_);

  // A door movement sets changed_, firing the state callback while the request is still waiting.
  fixture.report_broadcast(door_broadcast(0x0064, 0x0100));

  EXPECT_TRUE(fixture.door.light_requested_);
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

// A lamp that is no longer reported leaves nothing to judge a request against, so the request is dropped even
// with its toggle on the wire, and the entity follows the lamp once it is reported again.
TEST(HoermannHcpLightPlatformTest, LampBecomingUnknownDropsTheRequest) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  EXPECT_EQ(poll_command(fixture.door).first, 0x0800);

  fixture.report_broadcast(make_registers({0x0000, 0x0064, 0x0100}));
  EXPECT_FALSE(fixture.door.light_requested_);
  EXPECT_EQ(fixture.door.light_toggle_sent_at_, 0u);
  EXPECT_TRUE(fixture.output.status_has_warning());

  // The door ignored the toggle, and the entity goes back to what it reports.
  fixture.report_lamp(false);
  EXPECT_FALSE(fixture.entity_on());
  EXPECT_EQ(poll_command(fixture.door).first, 0x0000);
}

// A toggle the controller never fetches is dropped like a door command, so it cannot fire once polling resumes
// long afterwards, and the entity goes back to the lamp.
TEST(HoermannHcpLightPlatformTest, UnfetchedRequestIsDroppedAndNeverSent) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.light_requested_);
  EXPECT_TRUE(fixture.entity_on());

  // The controller keeps broadcasting but never polls, so the connection stays up.
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.report_lamp(false);
  ASSERT_TRUE(fixture.door.is_valid());

  EXPECT_FALSE(fixture.door.light_requested_);
  EXPECT_FALSE(fixture.entity_on());
  auto [idle, idle_2] = poll_command(fixture.door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
}

// The lamp is only reported some time after the toggle is fetched, so an unrelated door broadcast in
// that gap must not publish the state the lamp is about to leave.
TEST(HoermannHcpLightPlatformTest, DoorMovementDoesNotFlipTheEntityBeforeTheLampReports) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  poll_command(fixture.door);
  ASSERT_NE(fixture.door.light_toggle_sent_at_, 0u);
  ASSERT_FALSE(fixture.door.is_light_on());  // the lamp has still not been reported

  fixture.report_broadcast(door_broadcast(0x0064, 0x0100));

  EXPECT_TRUE(fixture.entity_on());
}

// Losing the bus controller discards the request too, so the entity must not keep showing it once the
// controller is back and still reporting the lamp unchanged.
TEST(HoermannHcpLightPlatformTest, RequestLostWithTheConnectionReturnsTheEntityToTheLamp) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.light_requested_);

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.pump();  // the connection times out and the request goes with it
  ASSERT_FALSE(fixture.door.is_valid());

  connect_controller(fixture.door);
  fixture.report_lamp(false);
  EXPECT_FALSE(fixture.entity_on());
  EXPECT_EQ(poll_command(fixture.door).first, 0x0000);
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

// Tapping on, off and on again while the first toggle is on its way ends on the last request, with no second
// toggle once the first lands.
TEST(HoermannHcpLightPlatformTest, QuickTapsSettleOnTheLastRequest) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  EXPECT_EQ(poll_command(fixture.door).first, 0x0800);
  fixture.command(false);
  EXPECT_FALSE(fixture.entity_on());
  fixture.command(true);
  EXPECT_TRUE(fixture.entity_on());
  EXPECT_EQ(poll_command(fixture.door).first, 0x0000);

  fixture.report_lamp(true);
  EXPECT_FALSE(fixture.door.light_requested_);
  EXPECT_TRUE(fixture.entity_on());
  EXPECT_EQ(poll_command(fixture.door).first, 0x0000);
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
  auto [toggle, toggle_2] = poll_command(fixture.door);
  EXPECT_EQ(toggle, 0x0800);
  EXPECT_EQ(toggle_2, 0x0200);
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
  EXPECT_FALSE(fixture.door.light_requested_);
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

// A reversing request once the toggle is fetched is a real request: the entity follows it at once, and the
// lamp is toggled back once the first toggle has landed.
TEST(HoermannHcpLightPlatformTest, ReversingRequestAfterFetchTogglesBackOnceTheFirstLands) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  EXPECT_EQ(poll_command(fixture.door).first, 0x0800);
  ASSERT_FALSE(fixture.door.is_light_on());

  fixture.command(false);
  EXPECT_FALSE(fixture.entity_on());
  EXPECT_EQ(poll_command(fixture.door).first, 0x0000);

  // The first toggle lands and is reported, but the entity is already heading for off.
  fixture.report_lamp(true);
  EXPECT_FALSE(fixture.entity_on());
  auto [toggle, toggle_2] = poll_command(fixture.door);
  EXPECT_EQ(toggle, 0x0800);
  EXPECT_EQ(toggle_2, 0x0200);

  // The second toggle lands too, and the lamp finally agrees with the request.
  fixture.report_lamp(false);
  EXPECT_FALSE(fixture.entity_on());
  EXPECT_FALSE(fixture.door.light_requested_);
}

// A refusal leaves no request behind, so it must not latch the entity against the next lamp change the door
// reports.
TEST(HoermannHcpLightPlatformTest, RefusalWithoutARequestStillFollowsTheLamp) {
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

// A door that takes the toggle but never actually switches the lamp must not leave the entity showing the
// request for ever; the wait has to end so the entity can settle back on what the door reports.
TEST(HoermannHcpLightPlatformTest, ToggleTheDoorIgnoresStopsBeingWaitedFor) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  fixture.command(true);
  consume_command(fixture.door);  // the door takes the toggle, then does nothing
  ASSERT_NE(fixture.door.light_toggle_sent_at_, 0u);
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
  fixture.report_broadcast(door_broadcast(0x0000, 0x4000));
  ASSERT_TRUE(fixture.door.is_light_known());

  fixture.command(true);
  auto [toggle, toggle_2] = poll_command(fixture.door);
  EXPECT_EQ(toggle, 0x0800);
  EXPECT_EQ(toggle_2, 0x0200);
}

// Reversing and repeating a request must not move the deadline of the toggle already on the wire, or a door
// that never reports the lamp would leave the entity waiting for ever.
TEST(HoermannHcpLightPlatformTest, ChangingTheRequestKeepsTheWatchdogArmed) {
  LightFixture fixture;
  fixture.door.connection_timeout_ms_ = 20;
  fixture.bring_up();

  fixture.command(true);
  consume_command(fixture.door);  // the toggle is fetched but never reported back
  const uint32_t sent_at = fixture.door.light_toggle_sent_at_;
  ASSERT_NE(sent_at, 0u);
  fixture.command(false);
  fixture.command(true);
  ASSERT_EQ(fixture.door.light_toggle_sent_at_, sent_at);

  // The door still says nothing about the lamp, so the wait has to time out on its own.
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  fixture.report_lamp(false);
  EXPECT_FALSE(fixture.door.light_requested_);
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

// Booting the entity on replays a lit state the door has never confirmed, so it has to be
// adopted back to what is known rather than turned into a command.
TEST(HoermannHcpLightPlatformTest, RestoredOnStateIsAdoptedNotCommanded) {
  LightFixture fixture{/*boot_on=*/true};
  connect_controller(fixture.door);
  fixture.settle();

  auto [idle, idle_2] = poll_command(fixture.door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
  EXPECT_FALSE(fixture.entity_on());
}

// A reversing press before the toggle is fetched takes the request back, so the lamp never moves.
TEST(HoermannHcpLightPlatformTest, ReversingPressBeforeTheFetchSendsNothing) {
  LightFixture fixture;
  fixture.bring_up();

  fixture.command(true);
  ASSERT_TRUE(fixture.door.light_requested_);

  fixture.command(false);
  EXPECT_FALSE(fixture.door.light_requested_);
  EXPECT_FALSE(fixture.entity_on());

  // Nothing is left for the controller to fetch, so the lamp stays off as asked.
  auto [idle, idle_2] = poll_command(fixture.door);
  EXPECT_EQ(idle, 0x0000);
  EXPECT_EQ(idle_2, 0x0000);
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
  // A broadcast without the lamp register leaves it unknown, so the request below is refused and the lamp
  // published back.
  fixture.door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x4000}));

  auto call = fixture.state.make_call();
  call.set_state(true);
  call.perform();
  fixture.state.loop();  // the refusal happens here and schedules the publish for a later pass

  // The lamp is reported again, switched on at the door, before that publish arrives.
  fixture.door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0010));
  fixture.settle();

  EXPECT_FALSE(fixture.door.light_requested_);
  EXPECT_TRUE(fixture.entity_on());
  EXPECT_EQ(poll_command(fixture.door).first, 0x0000);
}

}  // namespace esphome::hoermann_hcp::testing
