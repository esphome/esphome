#ifdef USE_HOST
#include <gtest/gtest.h>

#include "esphome/components/power_supply/power_supply.h"
#include "esphome/core/gpio.h"
#include "esphome/core/component.h"

namespace esphome::power_supply::testing {

TEST(PowerSupply, HasHigherPriorityThanBusWhenInternalAndEnableOnBoot) {
  power_supply::PowerSupply ps;
  InternalGPIOPin pin;
  ps.set_pin(&pin);
  ps.set_enable_on_boot(true);

  // POWER priority should be greater than BUS priority
  EXPECT_GT(ps.get_setup_priority(), setup_priority::BUS);
}

TEST(PowerSupply, FallsBackToIOWhenNotEnableOnBoot) {
  power_supply::PowerSupply ps;
  InternalGPIOPin pin;
  ps.set_pin(&pin);
  ps.set_enable_on_boot(false);

  EXPECT_EQ(ps.get_setup_priority(), setup_priority::IO);
}

}  // namespace esphome::power_supply::testing
#endif  // USE_HOST
