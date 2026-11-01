#include <gtest/gtest.h>

#include "esphome/components/cst226/touchscreen/cst226_touchscreen.h"
#include "../common.h"

namespace esphome::cst226::testing {

TEST(CST226TouchscreenTest, SetupCompletes) {
  StatusBus bus;
  CST226Touchscreen hub;
  hub.set_i2c_bus(&bus);
  hub.setup();
  EXPECT_TRUE(hub.can_proceed());
  EXPECT_FALSE(hub.is_failed());
}

TEST(CST226TouchscreenTest, SetupFailsWithoutTheChip) {
  StatusBus bus;
  bus.fail = true;
  CST226Touchscreen hub;
  hub.set_i2c_bus(&bus);
  hub.setup();
  EXPECT_TRUE(hub.is_failed());
}

}  // namespace esphome::cst226::testing
