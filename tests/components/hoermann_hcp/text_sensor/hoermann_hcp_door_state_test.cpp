#include <gtest/gtest.h>

#include "esphome/components/hoermann_hcp/text_sensor/hoermann_hcp_text_sensor.h"

#include "../common.h"

namespace esphome::hoermann_hcp::testing {

namespace {

// A status broadcast with the door state in the high byte of its third register.
void broadcast_state(HoermannHcp &door, uint16_t state_reg) {
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, state_reg}));
  door.update();
}

struct DoorStateFixture {
  DoorStateFixture() {
    this->sensor.setup();
    this->sensor.add_on_state_callback([this](const std::string & /*state*/) { this->publishes++; });
  }

  TestableHoermannHcp door;
  HoermannHcpDoorStateTextSensor sensor{&door};
  int publishes{0};
};

}  // namespace

// Polls and bus scans make the connection valid before any broadcast has said where the door is, and an
// undecodable state says nothing either. None of these may show the default state.
TEST(HoermannHcpDoorStateTest, NothingBeforeTheDoorReportsAState) {
  DoorStateFixture fixture;
  auto &door = fixture.door;
  door.update();
  EXPECT_FALSE(fixture.sensor.has_state());

  status_answer(door, 0x0003);
  door.update();
  ASSERT_TRUE(door.is_valid());
  EXPECT_FALSE(fixture.sensor.has_state());

  RegisterValues scan;
  door.on_read_holding_registers(STATE_REG, 5, scan);
  door.update();
  EXPECT_FALSE(fixture.sensor.has_state());

  broadcast_state(door, 0x1000);
  EXPECT_FALSE(fixture.sensor.has_state());

  // The first real state is shown even when it equals the default.
  broadcast_state(door, 0x4000);
  EXPECT_EQ(fixture.sensor.get_state(), "Closed");
}

// Every state the door reports is shown, including the vent and half-open positions and the moves to them.
TEST(HoermannHcpDoorStateTest, FollowsTheDoorState) {
  DoorStateFixture fixture;
  auto &door = fixture.door;
  connect_controller(door);

  const std::pair<uint16_t, const char *> states[] = {
      {0x2000, "Open"},          {0x0200, "Closing"},        {0x4000, "Closed"},    {0x0900, "Moving to vent"},
      {0x0A00, "Vent position"}, {0x0500, "Moving to half"}, {0x8000, "Half open"}, {0x0100, "Opening"},
      {0x0000, "Stopped"},       {0x0061, "Vent position"},
  };
  for (const auto &[reg, text] : states) {
    broadcast_state(door, reg);
    EXPECT_EQ(fixture.sensor.get_state(), text) << "state register 0x" << std::hex << reg;
  }
}

// Any hub change runs the publish path, so an unchanged door state is not published again, and a state the
// door is not known to report keeps the last one.
TEST(HoermannHcpDoorStateTest, EachStateIsPublishedOnce) {
  DoorStateFixture fixture;
  auto &door = fixture.door;
  connect_controller(door);
  broadcast_state(door, 0x2000);
  ASSERT_EQ(fixture.publishes, 1);

  // The lamp changes, the door state does not.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x2000, 0x0000, 0x0000, 0x0000, 0x0010}));
  door.update();
  EXPECT_EQ(fixture.publishes, 1);

  broadcast_state(door, 0x1000);
  EXPECT_EQ(fixture.publishes, 1);
  EXPECT_EQ(fixture.sensor.get_state(), "Open");
}

// While the bus controller is gone the last state stays. Once it is back, a poll alone shows nothing new; the
// next broadcast is published again even if it repeats the old state.
TEST(HoermannHcpDoorStateTest, LastStateStaysUntilTheNextBroadcast) {
  DoorStateFixture fixture;
  auto &door = fixture.door;
  connect_controller(door);
  broadcast_state(door, 0x8000);
  ASSERT_EQ(fixture.publishes, 1);

  door.set_valid_(false);
  door.update();
  EXPECT_EQ(fixture.sensor.get_state(), "Half open");

  connect_controller(door);
  door.update();
  EXPECT_EQ(fixture.publishes, 1);

  broadcast_state(door, 0x8000);
  EXPECT_EQ(fixture.publishes, 2);
  EXPECT_EQ(fixture.sensor.get_state(), "Half open");
}

// A sensor set up after the hub already decoded a state shows it right away.
TEST(HoermannHcpDoorStateTest, LateSetupShowsTheCurrentState) {
  TestableHoermannHcp door;
  connect_controller(door);
  broadcast_state(door, 0x0A00);

  HoermannHcpDoorStateTextSensor sensor(&door);
  sensor.setup();
  EXPECT_EQ(sensor.get_state(), "Vent position");
}

}  // namespace esphome::hoermann_hcp::testing
