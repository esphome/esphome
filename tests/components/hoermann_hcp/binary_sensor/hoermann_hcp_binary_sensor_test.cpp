#include <gtest/gtest.h>

#include "esphome/components/hoermann_hcp/binary_sensor/hoermann_hcp_binary_sensor.h"

namespace esphome::hoermann_hcp {

using modbus::RegisterValues;

namespace {

constexpr uint16_t COMMAND_REG = 0x9C41;
constexpr uint16_t BROADCAST_REG = 0x9D31;

RegisterValues make_registers(std::initializer_list<uint16_t> values) {
  RegisterValues registers;
  for (uint16_t value : values)
    registers.push_back(value);
  return registers;
}

// Exposes the connection bookkeeping so a drop can be driven without waiting one out.
class TestableHoermannHcp : public HoermannHcp {
 public:
  using HoermannHcp::set_valid_;
};

}  // namespace

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

  door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000}));
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

  door.on_write_registers(COMMAND_REG, make_registers({0x0000, 0x0000}));
  door.update();
  ASSERT_EQ(publishes, 1);

  // A status broadcast changes the door state without touching the connection.
  door.on_write_registers(BROADCAST_REG, make_registers({0x0000, 0x0064, 0x0100}));
  door.update();
  EXPECT_EQ(publishes, 1);
}

}  // namespace esphome::hoermann_hcp
