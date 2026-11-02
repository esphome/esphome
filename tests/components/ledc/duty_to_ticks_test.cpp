#include <gtest/gtest.h>

#include "esphome/components/ledc/ledc_duty.h"

namespace esphome::ledc::testing {

TEST(LedcDutyToTicksTest, FullScaleIsOnePastTheHighestLevel) {
  EXPECT_EQ(full_scale_ticks(8, 0), 256U);
  EXPECT_EQ(full_scale_ticks(8, 4), 4096U);
  EXPECT_EQ(duty_to_ticks(1.0f, 8, 0), full_scale_ticks(8, 0));
  EXPECT_EQ(duty_to_ticks(1.0f, 8, 4), full_scale_ticks(8, 4));
}

TEST(LedcDutyToTicksTest, ScalesAgainstThePwmPeriod) {
  EXPECT_EQ(duty_to_ticks(0.0f, 8, 0), 0U);
  EXPECT_EQ(duty_to_ticks(0.5f, 8, 0), 128U);
  EXPECT_EQ(duty_to_ticks(0.25f, 10, 0), 256U);
  EXPECT_EQ(duty_to_ticks(1.0f, 8, 0), 256U);
}

TEST(LedcDutyToTicksTest, HighestWholeTickIsReachable) {
  EXPECT_EQ(duty_to_ticks(255.0f / 256.0f, 8, 0), 255U);
  // Rounds to the nearest tick on either side of the last half step
  EXPECT_EQ(duty_to_ticks(255.4f / 256.0f, 8, 0), 255U);
  EXPECT_EQ(duty_to_ticks(255.6f / 256.0f, 8, 0), 256U);
}

TEST(LedcDutyToTicksTest, DitheringKeepsFractionalTicks) {
  // 8 bits plus 4 fractional bits: 4096 sixteenths of a tick per period
  EXPECT_EQ(duty_to_ticks(0.0f, 8, 4), 0U);
  EXPECT_EQ(duty_to_ticks(0.5f, 8, 4), 2048U);
  EXPECT_EQ(duty_to_ticks(100.25f / 256.0f, 8, 4), 1604U);
  EXPECT_EQ(duty_to_ticks(1.0f, 8, 4), 4096U);
}

TEST(LedcDutyToTicksTest, DitheringSnapsAboveTheLastWholeTick) {
  // The last whole tick is 255 * 16 = 4080; the switch point to full scale is 4088
  EXPECT_EQ(duty_to_ticks(4080.0f / 4096.0f, 8, 4), 4080U);
  EXPECT_EQ(duty_to_ticks(4081.0f / 4096.0f, 8, 4), 4080U);
  EXPECT_EQ(duty_to_ticks(4087.6f / 4096.0f, 8, 4), 4080U);
  EXPECT_EQ(duty_to_ticks(4088.0f / 4096.0f, 8, 4), 4096U);
  EXPECT_EQ(duty_to_ticks(4095.0f / 4096.0f, 8, 4), 4096U);
}

TEST(LedcDutyToTicksTest, HighestBitDepthFits) {
  // 20 bits plus 4 fractional bits still fit the float mantissa
  EXPECT_EQ(duty_to_ticks(1.0f, 20, 4), 1U << 24);
  EXPECT_EQ(duty_to_ticks(0.5f, 20, 4), 1U << 23);
  EXPECT_EQ(duty_to_ticks(0.5f, 20, 0), 1U << 19);
}

}  // namespace esphome::ledc::testing
