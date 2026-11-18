#include <utility>

#include <gtest/gtest.h>

#include "esphome/components/hoermann_hcp/binary_sensor/hoermann_hcp_binary_sensor.h"

#include "../common.h"

namespace esphome::hoermann_hcp::testing {

// Nothing has been heard from the bus controller yet, so the sensor starts out seeded as disconnected.
TEST(HoermannHcpBinarySensorTest, StartsDisconnected) {
  HoermannHcp door;
  HoermannHcpConnectedBinarySensor sensor(&door);
  sensor.setup();
  EXPECT_TRUE(sensor.has_state());
  EXPECT_FALSE(sensor.state);
}

// The connection flag follows the bus controller in both directions.
TEST(HoermannHcpBinarySensorTest, FollowsTheConnectionState) {
  TestableHoermannHcp door;
  HoermannHcpConnectedBinarySensor sensor(&door);
  sensor.setup();
  ASSERT_FALSE(sensor.state);

  connect_controller(door);
  door.update();
  EXPECT_TRUE(sensor.state);

  door.set_valid_(false);
  door.update();
  EXPECT_FALSE(sensor.state);
}

// Any hub change re-runs the publish path, so an unchanged connection must not be reported twice.
TEST(HoermannHcpBinarySensorTest, UnchangedConnectionIsPublishedOnce) {
  HoermannHcp door;
  HoermannHcpConnectedBinarySensor sensor(&door);
  sensor.setup();
  int publishes = 0;
  sensor.add_on_state_callback([&publishes](bool /*state*/) { publishes++; });

  connect_controller(door);
  door.update();
  ASSERT_EQ(publishes, 1);

  // A status broadcast changes the door state without touching the connection.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0100}));
  door.update();
  EXPECT_EQ(publishes, 1);
}

// Nothing has reported the flag yet, so the sensor stays unknown rather than claiming the motor is fine.
TEST(HoermannHcpActuatorErrorTest, UnknownUntilReported) {
  HoermannHcp door;
  binary_sensor::BinarySensor sensor;
  door.set_actuator_error_binary_sensor(&sensor);

  connect_controller(door);
  door.update();
  EXPECT_FALSE(sensor.has_state());
  // A broadcast without register 6 does not report it either.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x0000}));
  door.update();
  EXPECT_FALSE(sensor.has_state());
}

// Either bit of 0x30 in the high byte of register 6 is an error. Nothing else in the register is: 0x0200 is set on
// some installations, and the bits above 0x30 are not part of the flag.
TEST(HoermannHcpActuatorErrorTest, FollowsTheErrorBits) {
  HoermannHcp door;
  binary_sensor::BinarySensor sensor;
  door.set_actuator_error_binary_sensor(&sensor);

  const std::pair<uint16_t, bool> cases[] = {
      {0x0000, false}, {0x1000, true},  {0x0010, false}, {0x2000, true},  {0x0200, false},
      {0x3014, true},  {0x0214, false}, {0x4000, false}, {0x8000, false}, {0xC0FF, false},
  };
  for (const auto &[reg, error] : cases) {
    door.on_write_registers(BROADCAST_REG, lamp_broadcast(reg));
    ASSERT_TRUE(sensor.has_state()) << std::hex << reg;
    EXPECT_EQ(sensor.state, error) << std::hex << reg;
  }
}

// A quiet bus or a broadcast without register 6 keeps the last value, which the next report replaces.
TEST(HoermannHcpActuatorErrorTest, KeepsTheLastValueWhileNotReported) {
  TestableHoermannHcp door;
  binary_sensor::BinarySensor sensor;
  door.set_actuator_error_binary_sensor(&sensor);

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x1000));
  ASSERT_TRUE(sensor.state);
  door.set_valid_(false);
  door.update();
  EXPECT_TRUE(sensor.has_state());
  EXPECT_TRUE(sensor.state);

  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0000, 0x0000}));
  door.update();
  EXPECT_TRUE(sensor.state);

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x0000));
  EXPECT_FALSE(sensor.state);
}

// An unchanged flag is not reported again while the lamp next to it switches.
TEST(HoermannHcpActuatorErrorTest, LampChangeDoesNotRepublish) {
  HoermannHcp door;
  binary_sensor::BinarySensor sensor;
  door.set_actuator_error_binary_sensor(&sensor);
  int publishes = 0;
  sensor.add_on_state_callback([&publishes](bool /*state*/) { publishes++; });

  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x1000));
  ASSERT_EQ(publishes, 1);
  door.on_write_registers(BROADCAST_REG, lamp_broadcast(0x1010));
  EXPECT_EQ(publishes, 1);
}

}  // namespace esphome::hoermann_hcp::testing
