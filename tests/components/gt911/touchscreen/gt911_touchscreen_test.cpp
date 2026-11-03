#include <gtest/gtest.h>

#include "esphome/components/gt911/touchscreen/gt911_touchscreen.h"
#include "../common.h"

namespace esphome::gt911::testing {

TEST(GT911TouchscreenTest, SetupCompletes) {
  RegisterBus bus;
  bus.registers[0x8048] = {0x40, 0x01, 0xF0, 0x00};
  GT911Touchscreen hub;
  hub.set_i2c_bus(&bus);
  hub.set_i2c_address(0x5D);
  hub.setup();
  EXPECT_TRUE(hub.can_proceed());
  EXPECT_FALSE(hub.is_failed());
}

TEST(GT911TouchscreenTest, SetupFailsWithoutTheChip) {
  RegisterBus bus;
  bus.fail = true;
  GT911Touchscreen hub;
  hub.set_i2c_bus(&bus);
  hub.set_i2c_address(0x5D);
  hub.setup();
  EXPECT_FALSE(hub.can_proceed());
  EXPECT_TRUE(hub.is_failed());
}

}  // namespace esphome::gt911::testing
