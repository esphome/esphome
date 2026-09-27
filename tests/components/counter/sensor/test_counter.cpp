#include "../common.h"

namespace esphome::counter::testing {

TEST_F(CounterTest, SetValuePublishesState) {
  this->counter_.set_value(42);
  EXPECT_EQ(this->counter_.state, 42.0f);
}

TEST_F(CounterTest, IncrementDefaultsToOne) {
  this->counter_.increment();
  this->counter_.increment();
  EXPECT_EQ(this->counter_.state, 2.0f);
}

TEST_F(CounterTest, IncrementAcceptsNegativeAmounts) {
  this->counter_.set_value(10);
  this->counter_.increment(-25);
  EXPECT_EQ(this->counter_.state, -15.0f);
}

TEST_F(CounterTest, ValueBeyondInt32) {
  this->counter_.set_value(5000000000LL);
  this->counter_.increment(5000000000LL);
  EXPECT_EQ(this->counter_.state, 1.0e10f);
}

TEST_F(CounterTest, IncrementWrapsAtInt64Limits) {
  this->counter_.set_value(INT64_MAX_VALUE);
  this->counter_.increment(1);
  EXPECT_EQ(this->counter_.state, static_cast<float>(INT64_MIN_VALUE));

  this->counter_.set_value(INT64_MIN_VALUE);
  this->counter_.increment(-1);
  EXPECT_EQ(this->counter_.state, static_cast<float>(INT64_MAX_VALUE));
}

TEST_F(CounterTest, CountsEachPublishFromSource) {
  sensor::Sensor source;
  this->counter_.count_updates_from(&source);

  // The counted value is unrelated to what the source publishes.
  source.publish_state(10.0f);
  source.publish_state(10.0f);
  source.publish_state(-3.5f);
  EXPECT_EQ(this->counter_.state, 3.0f);
}

TEST_F(CounterTest, SourceUpdatesAddToCurrentValue) {
  sensor::Sensor source;
  this->counter_.count_updates_from(&source);

  this->counter_.set_value(100);
  source.publish_state(1.0f);
  EXPECT_EQ(this->counter_.state, 101.0f);
}

}  // namespace esphome::counter::testing
