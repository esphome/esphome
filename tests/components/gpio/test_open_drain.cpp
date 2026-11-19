#include "esphome/core/gpio.h"
#include <gtest/gtest.h>

namespace esphome::gpio::testing {

TEST(OpenDrainPinTest, PhysicalLevelsIgnoreLogicalInversion) {
  for (const bool inverted : {false, true}) {
    host::HostGPIOPin pin;
    pin.set_pin(1);
    pin.set_inverted(inverted);
    ASSERT_TRUE(pin.setup_open_drain(true));
    auto isr = pin.to_isr();
    EXPECT_TRUE(isr.open_drain_read());
    isr.open_drain_pull_low();
    EXPECT_FALSE(isr.open_drain_read());
    isr.open_drain_release();
    EXPECT_TRUE(isr.open_drain_read());
  }
}

}  // namespace esphome::gpio::testing
