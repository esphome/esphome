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

// The walks pass include_internal, so test builds' unconstructed entities are never read

TEST(ComponentIterator, NotRunningMakesNoProgress) {
  RefusingIterator it;
  it.try_advance(BIG_BUDGET);
  EXPECT_TRUE(it.completed());
  EXPECT_EQ(it.begin_calls, 0);
  EXPECT_EQ(it.end_calls, 0);
}

TEST(ComponentIterator, CompletesInOneCallWithoutRefusals) {
  RefusingIterator it;
  it.begin(true);
  it.try_advance(BIG_BUDGET);
  EXPECT_TRUE(it.completed());
  EXPECT_EQ(it.begin_calls, 1);
  EXPECT_EQ(it.end_calls, 1);
}

TEST(ComponentIterator, StepBudgetIsHonored) {
  RefusingIterator it;
  it.begin(true);
  it.try_advance(1);
  EXPECT_EQ(it.begin_calls, 1);
  EXPECT_EQ(it.end_calls, 0);
  EXPECT_FALSE(it.completed());
}

TEST(ComponentIterator, RefusedStepStopsBatchAndRetriesSameStep) {
  RefusingIterator it;
  it.end_refusals = 3;
  it.begin(true);
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
  it.begin(true);
  it.try_advance(BIG_BUDGET);
  it.try_advance(BIG_BUDGET);
  EXPECT_EQ(it.begin_calls, 2);
  EXPECT_FALSE(it.completed());
  it.try_advance(BIG_BUDGET);
  EXPECT_TRUE(it.completed());
  EXPECT_EQ(it.begin_calls, 3);
}

class YieldingBeginIterator : public RefusingIterator {
 public:
  bool on_begin() override {
    this->begin_calls++;
    this->yield_after_step_();
    return true;
  }
};

TEST(ComponentIterator, YieldEndsThePassAndTheNextResumes) {
  YieldingBeginIterator it;
  it.begin(true);
  it.try_advance(BIG_BUDGET);
  EXPECT_EQ(it.begin_calls, 1);
  EXPECT_EQ(it.end_calls, 0);
  EXPECT_FALSE(it.completed());
  it.try_advance(BIG_BUDGET);
  EXPECT_TRUE(it.completed());
  EXPECT_EQ(it.begin_calls, 1);
  EXPECT_EQ(it.end_calls, 1);
}

// The deprecated advance() wrapper must keep the legacy once-per-loop
// pattern working during the deprecation window.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
TEST(ComponentIterator, DeprecatedAdvanceKeepsLegacyPatternWorking) {
  RefusingIterator it;
  it.end_refusals = 2;
  it.begin(true);
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
static sensor::Sensor sensor_a;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static sensor::Sensor sensor_b;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static sensor::Sensor *const TEST_SENSORS[] = {&sensor_a, &sensor_b};
static const AppList<sensor::Sensor, 2> TEST_LIST(TEST_SENSORS);

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
  bool step_list() { return this->process_platform_item_(TEST_LIST, &ComponentIterator::on_sensor); }
  uint16_t at() const { return this->at_; }
  bool yield_requested() const { return this->yield_requested_; }

  sensor::Sensor *last_sensor{nullptr};
  int sensor_calls{0};
  int sensor_refusals{0};
  bool yield_on_sensor{false};
};

TEST(ComponentIteratorList, RefusedItemIsReofferedNotSkipped) {
  ItemRefusingIterator it;
  it.sensor_refusals = 2;
  EXPECT_FALSE(it.step_list());
  EXPECT_FALSE(it.step_list());
  EXPECT_EQ(it.at(), 0);
  EXPECT_EQ(it.last_sensor, &sensor_a);
  EXPECT_TRUE(it.step_list());
  EXPECT_EQ(it.at(), 1);
  EXPECT_TRUE(it.step_list());
  EXPECT_EQ(it.last_sensor, &sensor_b);
  EXPECT_EQ(it.sensor_calls, 4);
  EXPECT_TRUE(it.step_list());
  EXPECT_EQ(it.at(), 0);
  EXPECT_EQ(it.sensor_calls, 4);
}

TEST(ComponentIteratorList, YieldAfterStepIsRequestedPerItem) {
  ItemRefusingIterator it;
  it.yield_on_sensor = true;
  EXPECT_TRUE(it.step_list());
  EXPECT_TRUE(it.yield_requested());
  EXPECT_EQ(it.at(), 1);
  EXPECT_EQ(it.last_sensor, &sensor_a);
}

TEST(AppListView, WalksTheConstantArrayInOrder) {
  EXPECT_EQ(TEST_LIST.size(), 2u);
  EXPECT_FALSE(TEST_LIST.empty());
  EXPECT_EQ(TEST_LIST[0], &sensor_a);
  EXPECT_EQ(TEST_LIST[1], &sensor_b);
  std::vector<sensor::Sensor *> seen;
  for (auto *obj : TEST_LIST)
    seen.push_back(obj);
  EXPECT_EQ(seen, (std::vector<sensor::Sensor *>{&sensor_a, &sensor_b}));
}

TEST(AppListView, EmptyListHasNoItems) {
  const AppList<sensor::Sensor, 0> empty(nullptr);
  EXPECT_EQ(empty.size(), 0u);
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(empty.begin(), empty.end());
}
#endif  // USE_SENSOR

}  // namespace esphome::testing
