#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/sensor/filter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::sensor::testing {

static optional<float> double_or_drop(float x) {
  if (x < 0.0f)
    return {};
  return x * 2.0f;
}

// The filter has no bytes beyond its base
static_assert(sizeof(ApplyFilter<double_or_drop>) == sizeof(Filter));

TEST(SensorApplyFilter, PublishesWhatTheFunctionReturns) {
  Sensor sensor;
  ApplyFilter<double_or_drop> filter;
  sensor.add_filter(&filter);
  std::vector<float> seen;
  sensor.add_on_state_callback([&seen](float state) { seen.push_back(state); });

  sensor.publish_state(1.5f);
  sensor.publish_state(-1.0f);
  sensor.publish_state(4.0f);

  EXPECT_EQ(seen, (std::vector<float>{3.0f, 8.0f}));
  EXPECT_EQ(sensor.get_raw_state(), 4.0f);
}

}  // namespace esphome::sensor::testing
