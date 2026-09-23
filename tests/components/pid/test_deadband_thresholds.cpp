#include <gtest/gtest.h>

#include "esphome/components/pid/pid_climate.h"

namespace esphome::pid {

TEST(PIDClimateDeadbandThresholds, InvalidThresholdsDoNotChangeController) {
  PIDClimate climate;
  climate.set_threshold_low(-1.0f);
  climate.set_threshold_high(1.0f);

  EXPECT_FALSE(climate.set_deadband_thresholds(2.0f, 1.0f));

  EXPECT_FLOAT_EQ(climate.get_threshold_low(), -1.0f);
  EXPECT_FLOAT_EQ(climate.get_threshold_high(), 1.0f);
}

TEST(PIDClimateDeadbandThresholds, ValidThresholdsChangeController) {
  PIDClimate climate;
  climate.set_threshold_low(-1.0f);
  climate.set_threshold_high(1.0f);

  EXPECT_TRUE(climate.set_deadband_thresholds(-2.0f, 0.5f));

  EXPECT_FLOAT_EQ(climate.get_threshold_low(), -2.0f);
  EXPECT_FLOAT_EQ(climate.get_threshold_high(), 0.5f);
}

}  // namespace esphome::pid
