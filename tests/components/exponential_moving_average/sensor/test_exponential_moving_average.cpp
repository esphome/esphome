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

}  // namespace esphome::exponential_moving_average::testing
