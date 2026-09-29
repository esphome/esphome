#include <cmath>

#include "../common.h"

namespace esphome::exponential_moving_average::testing {

TEST_F(ExponentialMovingAverageTest, FirstValueStartsTheAverage) {
  TestableExponentialMovingAverageSensor ema(&this->source_);
  ema.setup();
  EXPECT_FALSE(ema.has_state());

  ema.process_(10.0f, 0);
  EXPECT_FLOAT_EQ(ema.state, 10.0f);
}

TEST_F(ExponentialMovingAverageTest, AlphaWeightsEachValue) {
  TestableExponentialMovingAverageSensor ema(&this->source_);
  ema.set_alpha(0.5f);
  ema.setup();

  ema.process_(10.0f, 0);
  ema.process_(20.0f, 0);
  EXPECT_FLOAT_EQ(ema.state, 15.0f);
  ema.process_(20.0f, 0);
  EXPECT_FLOAT_EQ(ema.state, 17.5f);
}

TEST_F(ExponentialMovingAverageTest, NanValuesAreIgnored) {
  TestableExponentialMovingAverageSensor ema(&this->source_);
  ema.set_alpha(0.5f);
  ema.setup();

  ema.process_(10.0f, 0);
  ema.process_(NAN, 0);
  EXPECT_FLOAT_EQ(ema.state, 10.0f);
  ema.process_(20.0f, 0);
  EXPECT_FLOAT_EQ(ema.state, 15.0f);
}

TEST_F(ExponentialMovingAverageTest, FollowsSourceSensor) {
  TestableExponentialMovingAverageSensor ema(&this->source_);
  ema.set_alpha(0.25f);
  ema.setup();

  this->source_.publish_state(8.0f);
  this->source_.publish_state(0.0f);
  EXPECT_FLOAT_EQ(ema.state, 6.0f);
}

TEST_F(ExponentialMovingAverageTest, TimeConstantWeightsByElapsedTime) {
  TestableExponentialMovingAverageSensor ema(&this->source_);
  ema.set_time_constant(1000);
  ema.setup();

  ema.process_(0.0f, 0);
  ema.process_(1.0f, 1000);
  EXPECT_NEAR(ema.state, 1.0f - std::exp(-1.0f), 1e-5f);
}

TEST_F(ExponentialMovingAverageTest, TimeConstantResultDoesNotDependOnSampleRate) {
  TestableExponentialMovingAverageSensor fast(&this->source_);
  fast.set_time_constant(1000);
  fast.set_restore(false);
  fast.setup();
  fast.process_(0.0f, 0);
  for (uint32_t t = 100; t <= 1000; t += 100)
    fast.process_(1.0f, t);

  TestableExponentialMovingAverageSensor slow(&this->source_);
  slow.set_time_constant(1000);
  slow.set_restore(false);
  slow.setup();
  slow.process_(0.0f, 0);
  slow.process_(1.0f, 1000);

  EXPECT_NEAR(fast.state, slow.state, 1e-5f);
}

TEST_F(ExponentialMovingAverageTest, TimeConstantIgnoresRepeatAtSameTime) {
  TestableExponentialMovingAverageSensor ema(&this->source_);
  ema.set_time_constant(1000);
  ema.setup();

  ema.process_(5.0f, 0);
  ema.process_(100.0f, 0);
  EXPECT_FLOAT_EQ(ema.state, 5.0f);
}

TEST_F(ExponentialMovingAverageTest, TimeConstantHandlesTimerWraparound) {
  TestableExponentialMovingAverageSensor ema(&this->source_);
  ema.set_time_constant(1000);
  ema.setup();

  ema.process_(0.0f, UINT32_MAX - 499);
  ema.process_(1.0f, 500);
  EXPECT_NEAR(ema.state, 1.0f - std::exp(-1.0f), 1e-5f);
}

TEST_F(ExponentialMovingAverageTest, ResetStartsANewAverage) {
  TestableExponentialMovingAverageSensor ema(&this->source_);
  ema.set_alpha(0.5f);
  ema.setup();

  ema.process_(10.0f, 0);
  ema.reset();
  EXPECT_TRUE(std::isnan(ema.state));
  ema.process_(40.0f, 0);
  EXPECT_FLOAT_EQ(ema.state, 40.0f);
}

TEST_F(ExponentialMovingAverageTest, AverageIsRestoredAfterReboot) {
  {
    TestableExponentialMovingAverageSensor before(&this->source_);
    before.set_alpha(0.5f);
    before.setup();
    before.process_(10.0f, 0);
    before.process_(20.0f, 0);
  }

  TestableExponentialMovingAverageSensor after(&this->source_);
  after.set_alpha(0.5f);
  after.setup();
  ASSERT_TRUE(after.has_state());
  EXPECT_FLOAT_EQ(after.state, 15.0f);

  // Continues from the restored value rather than starting again.
  after.process_(25.0f, 0);
  EXPECT_FLOAT_EQ(after.state, 20.0f);
}

TEST_F(ExponentialMovingAverageTest, NothingRestoredWhenRestoreIsOff) {
  {
    TestableExponentialMovingAverageSensor before(&this->source_);
    before.setup();
    before.process_(10.0f, 0);
  }

  TestableExponentialMovingAverageSensor after(&this->source_);
  after.set_restore(false);
  after.setup();
  EXPECT_FALSE(after.has_state());
  after.process_(30.0f, 0);
  EXPECT_FLOAT_EQ(after.state, 30.0f);
}

TEST_F(ExponentialMovingAverageTest, ResetClearsTheSavedAverage) {
  {
    TestableExponentialMovingAverageSensor before(&this->source_);
    before.setup();
    before.process_(10.0f, 0);
    before.reset();
  }

  TestableExponentialMovingAverageSensor after(&this->source_);
  after.setup();
  EXPECT_FALSE(after.has_state());
}

TEST_F(ExponentialMovingAverageTest, PreviousWeightingCountsGapAtPreviousValue) {
  TestableExponentialMovingAverageSensor ema(&this->source_);
  ema.set_time_constant(1000);
  ema.set_time_weighting(TIME_WEIGHTING_PREVIOUS);
  ema.setup();

  // The value stayed at 20 for an hour before changing to 25.
  ema.process_(20.0f, 0);
  ema.process_(25.0f, 3600000);
  EXPECT_FLOAT_EQ(ema.state, 20.0f);

  // The 25 is counted over the following interval.
  ema.process_(25.0f, 3601000);
  EXPECT_NEAR(ema.state, 20.0f + 5.0f * (1.0f - std::exp(-1.0f)), 1e-4f);
}

TEST_F(ExponentialMovingAverageTest, NewWeightingCountsGapAtNewValue) {
  TestableExponentialMovingAverageSensor ema(&this->source_);
  ema.set_time_constant(1000);
  ema.setup();

  ema.process_(20.0f, 0);
  ema.process_(25.0f, 3600000);
  EXPECT_FLOAT_EQ(ema.state, 25.0f);
}

TEST_F(ExponentialMovingAverageTest, LinearWeightingFollowsStraightLine) {
  TestableExponentialMovingAverageSensor ema(&this->source_);
  ema.set_time_constant(1000);
  ema.set_time_weighting(TIME_WEIGHTING_LINEAR);
  ema.setup();

  // An average of a value rising steadily from 0 to 1 over one time constant ends at exp(-1).
  ema.process_(0.0f, 0);
  ema.process_(1.0f, 1000);
  EXPECT_NEAR(ema.state, std::exp(-1.0f), 1e-5f);
}

TEST_F(ExponentialMovingAverageTest, LinearWeightingMatchesManySmallSteps) {
  TestableExponentialMovingAverageSensor coarse(&this->source_);
  coarse.set_time_constant(1000);
  coarse.set_time_weighting(TIME_WEIGHTING_LINEAR);
  coarse.set_restore(false);
  coarse.setup();
  coarse.process_(0.0f, 0);
  coarse.process_(10.0f, 2000);

  TestableExponentialMovingAverageSensor fine(&this->source_);
  fine.set_time_constant(1000);
  fine.set_time_weighting(TIME_WEIGHTING_LINEAR);
  fine.set_restore(false);
  fine.setup();
  fine.process_(0.0f, 0);
  for (uint32_t t = 10; t <= 2000; t += 10)
    fine.process_(t / 200.0f, t);

  EXPECT_NEAR(coarse.state, fine.state, 1e-3f);
}

TEST_F(ExponentialMovingAverageTest, LinearWeightingIgnoresRepeatAtSameTime) {
  TestableExponentialMovingAverageSensor ema(&this->source_);
  ema.set_time_constant(1000);
  ema.set_time_weighting(TIME_WEIGHTING_LINEAR);
  ema.setup();

  ema.process_(5.0f, 0);
  ema.process_(100.0f, 0);
  EXPECT_FLOAT_EQ(ema.state, 5.0f);
}

TEST_F(ExponentialMovingAverageTest, FirstValueAfterRebootUsesNewValue) {
  {
    TestableExponentialMovingAverageSensor before(&this->source_);
    before.setup();
    before.process_(10.0f, 0);
  }

  // No reading from before the reboot is known, so the new value is used for the first interval.
  TestableExponentialMovingAverageSensor after(&this->source_);
  after.set_time_constant(1000);
  after.set_time_weighting(TIME_WEIGHTING_PREVIOUS);
  after.setup();
  after.process_(20.0f, 1000);
  EXPECT_NEAR(after.state, 10.0f + 10.0f * (1.0f - std::exp(-1.0f)), 1e-4f);
}

TEST(TimeWeightingTest, Names) {
  EXPECT_STREQ(LOG_STR_ARG(time_weighting_to_string(TIME_WEIGHTING_NEW)), "new");
  EXPECT_STREQ(LOG_STR_ARG(time_weighting_to_string(TIME_WEIGHTING_PREVIOUS)), "previous");
  EXPECT_STREQ(LOG_STR_ARG(time_weighting_to_string(TIME_WEIGHTING_LINEAR)), "linear");
}

struct ScaleDurationCase {
  uint32_t ms;
  float value;
  const char *unit;
  uint8_t decimals;
};

class ScaleDurationTest : public ::testing::TestWithParam<ScaleDurationCase> {};

TEST_P(ScaleDurationTest, PicksLargestUnitOfAtLeastOne) {
  const ScaleDurationCase &c = GetParam();
  const ScaledDuration scaled = scale_duration(c.ms);
  EXPECT_FLOAT_EQ(scaled.value, c.value);
  EXPECT_STREQ(LOG_STR_ARG(scaled.unit), c.unit);
  EXPECT_EQ(scaled.decimals, c.decimals);
}

INSTANTIATE_TEST_SUITE_P(
    Units, ScaleDurationTest,
    ::testing::Values(ScaleDurationCase{1, 1.0f, "ms", 0}, ScaleDurationCase{999, 999.0f, "ms", 0},
                      ScaleDurationCase{1000, 1.0f, "s", 1}, ScaleDurationCase{95000, 1.5833334f, "min", 1},
                      ScaleDurationCase{59999, 59.999f, "s", 1}, ScaleDurationCase{60000, 1.0f, "min", 1},
                      ScaleDurationCase{300000, 5.0f, "min", 1}, ScaleDurationCase{3599999, 59.999983f, "min", 1},
                      ScaleDurationCase{3600000, 1.0f, "h", 1}, ScaleDurationCase{86400000, 24.0f, "h", 1}));

}  // namespace esphome::exponential_moving_average::testing
