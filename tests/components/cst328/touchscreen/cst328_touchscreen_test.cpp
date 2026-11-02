#include <gtest/gtest.h>

#include "esphome/components/cst328/touchscreen/cst328_touchscreen.h"
#include "../common.h"

namespace esphome::cst328::testing {

class TestableCST328 : public CST328Touchscreen {
 public:
  using CST328Touchscreen::setup_complete_;
  using CST328Touchscreen::x_raw_max_;
  using CST328Touchscreen::y_raw_max_;
};

TEST(CST328TouchscreenTest, SetupReadsTheResolution) {
  RegisterBus bus;
  bus.registers[CST_REG_FW_CRC_AND_BOOT_TIME] = {0, 0, 0xCA, 0xCA};
  bus.registers[CST_REG_X_Y_RESOLUTION] = {0x40, 0x01, 0xF0, 0x00};
  TestableCST328 hub;
  hub.set_i2c_bus(&bus);
  hub.setup();
  EXPECT_TRUE(hub.setup_complete_);
  EXPECT_EQ(hub.x_raw_max_, 320);
  EXPECT_EQ(hub.y_raw_max_, 240);
}

TEST(CST328TouchscreenTest, SetupFailsOnAFirmwareCrcMismatch) {
  RegisterBus bus;
  TestableCST328 hub;
  hub.set_i2c_bus(&bus);
  hub.setup();
  EXPECT_FALSE(hub.setup_complete_);
  EXPECT_TRUE(hub.is_failed());
}

}  // namespace esphome::cst328::testing
