#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/binary_sensor/filter.h"

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

}  // namespace esphome::binary_sensor::testing
