#include <gtest/gtest.h>

#include "esphome/components/pid/pid_climate.h"

namespace esphome::pid {

TEST(PIDSetDeadbandThresholdParametersAction, InvalidThresholdsDoNotChangeController) {
  PIDClimate climate;
  climate.set_threshold_low(-1.0f);
  climate.set_threshold_high(1.0f);
  EXPECT_FLOAT_EQ(climate.get_threshold_low(), -1.0f);
  EXPECT_FLOAT_EQ(climate.get_threshold_high(), 1.0f);

  PIDSetDeadbandThresholdParametersAction<> action(&climate);
  action.set_threshold_low([]() -> float { return 2.0f; });
  action.set_threshold_high([]() -> float { return 1.0f; });
  action.play();

  EXPECT_FLOAT_EQ(climate.get_threshold_low(), -1.0f);
  EXPECT_FLOAT_EQ(climate.get_threshold_high(), 1.0f);
}

TEST(PIDSetDeadbandThresholdParametersAction, ValidThresholdsChangeController) {
  PIDClimate climate;
  climate.set_threshold_low(-1.0f);
  climate.set_threshold_high(1.0f);
  EXPECT_FLOAT_EQ(climate.get_threshold_low(), -1.0f);
  EXPECT_FLOAT_EQ(climate.get_threshold_high(), 1.0f);

  PIDSetDeadbandThresholdParametersAction<> action(&climate);
  action.set_threshold_low([]() -> float { return -2.0f; });
  action.set_threshold_high([]() -> float { return 0.5f; });
  action.play();

  EXPECT_FLOAT_EQ(climate.get_threshold_low(), -2.0f);
  EXPECT_FLOAT_EQ(climate.get_threshold_high(), 0.5f);
}

TEST(PIDSetDeadbandThresholdParametersAction, EqualThresholdsDisableDeadband) {
  PIDClimate climate;
  climate.set_threshold_low(-1.0f);
  climate.set_threshold_high(1.0f);
  EXPECT_FLOAT_EQ(climate.get_threshold_low(), -1.0f);
  EXPECT_FLOAT_EQ(climate.get_threshold_high(), 1.0f);

  PIDSetDeadbandThresholdParametersAction<> action(&climate);
  action.set_threshold_low([]() -> float { return 0.0f; });
  action.set_threshold_high([]() -> float { return 0.0f; });
  action.play();

  EXPECT_FLOAT_EQ(climate.get_threshold_low(), 0.0f);
  EXPECT_FLOAT_EQ(climate.get_threshold_high(), 0.0f);
  EXPECT_FALSE(climate.in_deadband());
}

TEST(PIDSetDeadbandControlParametersMultipliersAction, ChangesAllMultipliers) {
  PIDClimate climate;
  climate.set_kp_multiplier(0.4f);
  climate.set_ki_multiplier(0.5f);
  climate.set_kd_multiplier(0.6f);
  EXPECT_FLOAT_EQ(climate.get_kp_multiplier(), 0.4f);
  EXPECT_FLOAT_EQ(climate.get_ki_multiplier(), 0.5f);
  EXPECT_FLOAT_EQ(climate.get_kd_multiplier(), 0.6f);

  PIDSetDeadbandControlParametersMultipliersAction<> action(&climate);
  action.set_kp_multiplier([]() -> float { return 0.1f; });
  action.set_ki_multiplier([]() -> float { return 0.2f; });
  action.set_kd_multiplier([]() -> float { return 0.3f; });
  action.play();

  EXPECT_FLOAT_EQ(climate.get_kp_multiplier(), 0.1f);
  EXPECT_FLOAT_EQ(climate.get_ki_multiplier(), 0.2f);
  EXPECT_FLOAT_EQ(climate.get_kd_multiplier(), 0.3f);
}

}  // namespace esphome::pid
