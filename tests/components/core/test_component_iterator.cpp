#include <gtest/gtest.h>

#include "esphome/core/component_iterator.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/application.h"
#endif

namespace esphome::testing {

// Iterator whose begin/end callbacks can refuse a configurable number of
// times; all entity callbacks accept (any registered entities are accepted).
class RefusingIterator : public ComponentIterator {
 public:
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper) \
  bool on_##singular(type *obj) override { return true; }
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  ENTITY_TYPE_(type, singular, plural, count, upper)
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
  // NOLINTEND(bugprone-macro-parentheses)

  bool on_begin() override { return step(this->begin_calls, this->begin_refusals); }
  bool on_end() override { return step(this->end_calls, this->end_refusals); }

  int begin_calls{0};
  int end_calls{0};
  int begin_refusals{0};
  int end_refusals{0};

 protected:
  static bool step(int &calls, int &refusals) {
    calls++;
    if (refusals > 0) {
      refusals--;
      return false;
    }
    return true;
  }
};

// Far above the fixed number of iterator states
static constexpr size_t BIG_BUDGET = 1000;

TEST(ComponentIterator, NotRunningMakesNoProgress) {
  RefusingIterator it;
  it.try_advance(BIG_BUDGET);
  EXPECT_TRUE(it.completed());
  EXPECT_EQ(it.begin_calls, 0);
  EXPECT_EQ(it.end_calls, 0);
}

TEST(ComponentIterator, CompletesInOneCallWithoutRefusals) {
  RefusingIterator it;
  it.begin();
  it.try_advance(BIG_BUDGET);
  EXPECT_TRUE(it.completed());
  EXPECT_EQ(it.begin_calls, 1);
  EXPECT_EQ(it.end_calls, 1);
}

TEST(ComponentIterator, StepBudgetIsHonored) {
  RefusingIterator it;
  it.begin();
  it.try_advance(1);
  EXPECT_EQ(it.begin_calls, 1);
  EXPECT_EQ(it.end_calls, 0);
  EXPECT_FALSE(it.completed());
}

TEST(ComponentIterator, RefusedStepStopsBatchAndRetriesSameStep) {
  RefusingIterator it;
  it.end_refusals = 3;
  it.begin();
  // First call runs until the refused end step, which stops the pass
  it.try_advance(BIG_BUDGET);
  EXPECT_EQ(it.end_calls, 1);
  EXPECT_FALSE(it.completed());
  // The refused step is retried once per call, not skipped
  it.try_advance(BIG_BUDGET);
  it.try_advance(BIG_BUDGET);
  EXPECT_EQ(it.end_calls, 3);
  EXPECT_FALSE(it.completed());
  // Once accepted, the iteration completes
  it.try_advance(BIG_BUDGET);
  EXPECT_TRUE(it.completed());
  EXPECT_EQ(it.end_calls, 4);
}

TEST(ComponentIterator, RefusedBeginStopsBatchAndRetries) {
  RefusingIterator it;
  it.begin_refusals = 2;
  it.begin();
  it.try_advance(BIG_BUDGET);
  it.try_advance(BIG_BUDGET);
  EXPECT_EQ(it.begin_calls, 2);
  EXPECT_FALSE(it.completed());
  it.try_advance(BIG_BUDGET);
  EXPECT_TRUE(it.completed());
  EXPECT_EQ(it.begin_calls, 3);
}

// The deprecated advance() wrapper must keep the legacy once-per-loop
// pattern working during the deprecation window.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
TEST(ComponentIterator, DeprecatedAdvanceKeepsLegacyPatternWorking) {
  RefusingIterator it;
  it.end_refusals = 2;
  it.begin();
  size_t guard = 0;
  while (!it.completed() && guard++ < BIG_BUDGET) {
    it.advance();
  }
  EXPECT_TRUE(it.completed());
  // Two refused end steps were retried, then accepted
  EXPECT_EQ(it.end_calls, 3);
}
#pragma GCC diagnostic pop

#ifdef USE_SENSOR
// Iterator whose sensor callback can refuse or yield; pins the per-item
// contract: a refused item is re-offered with at_ unchanged, never skipped.
class ItemRefusingIterator : public RefusingIterator {
 public:
  bool on_sensor(sensor::Sensor *obj) override {
    this->last_sensor = obj;
    if (!step(this->sensor_calls, this->sensor_refusals))
      return false;
    if (this->yield_on_sensor)
      this->yield_after_step_();
    return true;
  }
  sensor::Sensor *last_sensor{nullptr};
  int sensor_calls{0};
  int sensor_refusals{0};
  bool yield_on_sensor{false};
};

class ComponentIteratorSensorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static sensor::Sensor sensor_a;
    static sensor::Sensor sensor_b;
    static bool registered = false;
    if (!registered) {
      App.register_sensor(&sensor_a);
      App.register_sensor(&sensor_b);
      registered = true;
    }
    // StaticVector drops silently when full; fail the fixture, not the contract
    ASSERT_EQ(App.get_sensors().size(), 2u) << "benchmark.yaml sensor count too small";
  }
};

TEST_F(ComponentIteratorSensorTest, RefusedItemIsReofferedNotSkipped) {
  ItemRefusingIterator it;
  it.sensor_refusals = 2;
  it.begin();
  // Runs until the first sensor refuses
  it.try_advance(BIG_BUDGET);
  EXPECT_EQ(it.sensor_calls, 1);
  EXPECT_FALSE(it.completed());
  // The refused item is re-offered, not skipped
  it.try_advance(BIG_BUDGET);
  EXPECT_EQ(it.sensor_calls, 2);
  sensor::Sensor *refused = it.last_sensor;
  // Once accepted, iteration continues through the second sensor to the end
  it.try_advance(BIG_BUDGET);
  EXPECT_TRUE(it.completed());
  EXPECT_NE(it.last_sensor, refused);
  EXPECT_EQ(it.sensor_calls, 4);
}

TEST_F(ComponentIteratorSensorTest, YieldAfterStepEndsPassAndResumes) {
  ItemRefusingIterator it;
  it.yield_on_sensor = true;
  it.begin();
  // The pass ends right after the first sensor despite a big budget
  it.try_advance(BIG_BUDGET);
  EXPECT_EQ(it.sensor_calls, 1);
  EXPECT_FALSE(it.completed());
  // The next pass ends after the second sensor
  it.try_advance(BIG_BUDGET);
  EXPECT_EQ(it.sensor_calls, 2);
  // Remaining states then run to completion in one pass
  it.try_advance(BIG_BUDGET);
  EXPECT_TRUE(it.completed());
}
#endif  // USE_SENSOR

}  // namespace esphome::testing
