#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/binary_sensor/filter.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::binary_sensor::testing {

static optional<bool> invert_or_drop_on(Filter * /*self*/, bool x) {
  if (x)
    return {};
  return true;
}

// The filter has no bytes beyond its base
static_assert(sizeof(ApplyFilter<invert_or_drop_on>) == sizeof(Filter));

static Filter *last_self = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Outputs through the filter itself and stops the chain, like a filter that schedules its output
static optional<bool> output_through_self(Filter *self, bool x) {
  last_self = self;
  self->output(!x);
  return {};
}

TEST(BinarySensorApplyFilter, PublishesWhatTheFunctionReturns) {
  BinarySensor sensor;
  ApplyFilter<invert_or_drop_on> filter;
  sensor.add_filter(&filter);
  std::vector<bool> seen;
  sensor.add_on_state_callback([&seen](bool state) { seen.push_back(state); });

  sensor.publish_state(false);
  sensor.publish_state(true);

  EXPECT_EQ(seen, (std::vector<bool>{true}));
}

TEST(BinarySensorApplyFilter, PassesItselfToTheFunction) {
  BinarySensor sensor;
  ApplyFilter<output_through_self> filter;
  sensor.add_filter(&filter);

  sensor.publish_state(true);

  EXPECT_EQ(last_self, &filter);
  ASSERT_TRUE(sensor.has_state());
  EXPECT_FALSE(sensor.state);
}

static void run_scheduler_after(uint32_t ms) {
  delay(ms);
  App.scheduler.call(millis());
}

// Mirrors the code generated for `delayed_on: 10ms`
static optional<bool> delayed_on_10ms(Filter *filter, bool x) {
  return x ? delay_output(filter, 10, true) : cancel_and_pass(filter, false);
}

TEST(BinarySensorDelayOutput, UndelayedEdgeCancelsAndPassesAtOnce) {
  BinarySensor sensor;
  ApplyFilter<delayed_on_10ms> delayed_on;
  sensor.add_filter(&delayed_on);
  sensor.publish_state(true);
  EXPECT_FALSE(sensor.has_state());
  sensor.publish_state(false);
  ASSERT_TRUE(sensor.has_state());
  EXPECT_FALSE(sensor.state);
  run_scheduler_after(30);
  EXPECT_FALSE(sensor.state);
}

// Mirrors the code generated for `delayed_on_off: 0ms`
static optional<bool> delayed_on_off_0ms(Filter *filter, bool x) { return delay_output(filter, 0, x); }

// A 0 ms delay still goes through the scheduler, so edges within one pass coalesce
TEST(BinarySensorDelayOutput, ZeroDelayWaitsForTheSchedulerAndCoalesces) {
  BinarySensor sensor;
  ApplyFilter<delayed_on_off_0ms> delayed_on_off;
  sensor.add_filter(&delayed_on_off);
  int changes = 0;
  sensor.add_on_state_callback([&changes](bool) { changes++; });
  sensor.publish_state(true);
  sensor.publish_state(false);
  sensor.publish_state(true);
  EXPECT_FALSE(sensor.has_state());
  run_scheduler_after(1);
  ASSERT_TRUE(sensor.has_state());
  EXPECT_TRUE(sensor.state);
  EXPECT_EQ(changes, 1);
}

}  // namespace esphome::binary_sensor::testing
