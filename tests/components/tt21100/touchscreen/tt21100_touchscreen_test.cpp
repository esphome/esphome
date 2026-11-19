#include <gtest/gtest.h>

#include "esphome/components/tt21100/touchscreen/tt21100.h"
#include "../common.h"

namespace esphome::tt21100::testing {

TEST(TT21100TouchscreenTest, SetupDoesNotFail) {
  ReportBus bus;
  TT21100Touchscreen hub;
  hub.set_i2c_bus(&bus);
  hub.set_interrupt_pin(nullptr);
  hub.setup();
  EXPECT_FALSE(hub.is_failed());
}

}  // namespace esphome::tt21100::testing
