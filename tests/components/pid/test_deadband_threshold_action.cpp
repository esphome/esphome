#include <gtest/gtest.h>

#include "esphome/components/pid/pid_climate.h"

namespace esphome::pid {

TEST(PIDSetDeadbandThresholdParametersAction, InvalidThresholdsDoNotChangeController) {
  PIDClimate climate;
  climate.set_threshold_low(-1.0f);
  climate.set_threshold_high(1.0f);

  PIDSetDeadbandThresholdParametersAction<> action(&climate);
  action.set_threshold_low([]() -> float { return 2.0f; });
  action.set_threshold_high([]() -> float { return 1.0f; });
  action.play();

  EXPECT_FLOAT_EQ(climate.get_threshold_low(), -1.0f);
  EXPECT_FLOAT_EQ(climate.get_threshold_high(), 1.0f);
}

}  // namespace esphome::pid
