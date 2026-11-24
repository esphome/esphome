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

// Test builds never construct codegen entities: the walks pass include_internal so the iterator
// never reads them, and the callbacks below only look at the test's own objects.

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
// Iterator whose sensor callback can refuse or yield; pins the per-item
// contract: a refused item is re-offered with at_ unchanged, never skipped.
// The two sensors the tests register into the runtime slots
static sensor::Sensor sensor_a;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static sensor::Sensor sensor_b;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
// Registered with an explicit object_id hash and entity fields
static sensor::Sensor sensor_c;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static sensor::Sensor sensor_d;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static constexpr uint32_t SENSOR_C_HASH = 0x12345678;
static constexpr uint32_t SENSOR_C_FIELDS =
    (1u << ENTITY_FIELD_INTERNAL_SHIFT) | (uint32_t{ENTITY_CATEGORY_CONFIG} << ENTITY_FIELD_ENTITY_CATEGORY_SHIFT);

class ItemRefusingIterator : public RefusingIterator {
 public:
  bool on_sensor(sensor::Sensor *obj) override {
    // Codegen sensors are unconstructed in test builds; accept them untouched
    if (obj != &sensor_a && obj != &sensor_b)
      return true;
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
    static bool registered = false;
    if (!registered) {
      // Set by the owner before registration; registering must keep it
      sensor_b.set_internal(true);
      App.register_sensor(&sensor_a, "Runtime A");
      App.register_sensor(&sensor_b, "Runtime B");
      App.register_sensor(&sensor_c, "Runtime C", SENSOR_C_HASH, SENSOR_C_FIELDS);
      // Hash 0: computed from the name; fields 0 replace what the owner set
      sensor_d.set_internal(true);
      App.register_sensor(&sensor_d, "Runtime D", 0, 0);
      registered = true;
    }
    // The runtime slots follow the codegen sensors
    auto sensors = App.get_sensors();
    ASSERT_EQ(sensors.size(), ESPHOME_ENTITY_SENSOR_COUNT + 4u);
    ASSERT_EQ(sensors[ESPHOME_ENTITY_SENSOR_COUNT], &sensor_a);
    ASSERT_EQ(sensors[ESPHOME_ENTITY_SENSOR_COUNT + 1], &sensor_b);
    ASSERT_EQ(sensors[ESPHOME_ENTITY_SENSOR_COUNT + 2], &sensor_c);
    ASSERT_EQ(sensors[ESPHOME_ENTITY_SENSOR_COUNT + 3], &sensor_d);
  }
};

TEST_F(ComponentIteratorSensorTest, RegistrationPastTheReservationIsRefused) {
  static sensor::Sensor extra;
  extra.set_internal(true);
  EXPECT_FALSE(App.register_sensor(&extra, "Runtime E"));
  EXPECT_FALSE(App.register_sensor(&extra, "Runtime E", SENSOR_C_HASH, 0));
  EXPECT_EQ(App.get_sensors().size(), ESPHOME_ENTITY_SENSOR_COUNT + 4u);
  // Left untouched
  EXPECT_EQ(extra.get_object_id_hash(), 0u);
  EXPECT_TRUE(extra.get_name().empty());
  EXPECT_TRUE(extra.is_internal());
}

TEST_F(ComponentIteratorSensorTest, ExplicitHashAndFieldsAreApplied) {
  EXPECT_STREQ(sensor_c.get_name().c_str(), "Runtime C");
  EXPECT_EQ(sensor_c.get_object_id_hash(), SENSOR_C_HASH);
  EXPECT_TRUE(sensor_c.is_internal());
  EXPECT_EQ(sensor_c.get_entity_category(), ENTITY_CATEGORY_CONFIG);
}

TEST_F(ComponentIteratorSensorTest, ZeroHashComesFromTheNameAndFieldsReplaceTheOwners) {
  // esphome.helpers.fnv1_hash_object_id("Runtime D")
  EXPECT_EQ(sensor_d.get_object_id_hash(), 572462446u);
  EXPECT_FALSE(sensor_d.is_internal());
}

TEST_F(ComponentIteratorSensorTest, RegistrationKeepsTheEntityFields) {
  EXPECT_TRUE(sensor_b.is_internal());
  EXPECT_FALSE(sensor_a.is_internal());
}

TEST_F(ComponentIteratorSensorTest, IteratorOutlivesTheView) {
  // The view returned by get_sensors() is gone after this statement
  auto it = App.get_sensors().begin();
  for (size_t i = 0; i < ESPHOME_ENTITY_SENSOR_COUNT; i++)
    ++it;
  EXPECT_EQ(*it, &sensor_a);
  ++it;
  EXPECT_EQ(*it, &sensor_b);
}

TEST_F(ComponentIteratorSensorTest, RuntimeEntitiesAreNamedAndKeyed) {
  EXPECT_STREQ(sensor_a.get_name().c_str(), "Runtime A");
  EXPECT_TRUE(sensor_a.has_own_name());
  // What codegen computes for these names (esphome.helpers.fnv1_hash_object_id)
  EXPECT_EQ(sensor_a.get_object_id_hash(), 572462443u);
  EXPECT_EQ(sensor_b.get_object_id_hash(), 572462440u);
// The lookup reads every listed sensor; only without unconstructed codegen sensors
#if ESPHOME_ENTITY_SENSOR_COUNT == 0
#ifdef USE_DEVICES
  EXPECT_EQ(App.get_sensor_by_key(sensor_b.get_object_id_hash(), 0, true), &sensor_b);
#else
  EXPECT_EQ(App.get_sensor_by_key(sensor_b.get_object_id_hash(), true), &sensor_b);
#endif
#endif
}

TEST_F(ComponentIteratorSensorTest, RefusedItemIsReofferedNotSkipped) {
  ItemRefusingIterator it;
  it.sensor_refusals = 2;
  it.begin(true);
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
  it.begin(true);
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
