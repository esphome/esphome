#include <gtest/gtest.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/binary_sensor/filter.h"

namespace esphome::binary_sensor::testing {

TEST(BinarySensorInvalidate, RepeatedValuePassesTheFiltersAfterInvalidate) {
  BinarySensor sensor;
  InvertFilter invert;
  sensor.add_filter(&invert);
  sensor.publish_state(false);
  EXPECT_TRUE(sensor.state);

  sensor.invalidate_state();
  EXPECT_FALSE(sensor.has_state());
  sensor.publish_state(false);
  ASSERT_TRUE(sensor.has_state());
  EXPECT_TRUE(sensor.state);
}

TEST(BinarySensorInvalidate, PublishInitialStateRepeatsAFilteredValue) {
  BinarySensor sensor;
  InvertFilter invert;
  sensor.add_filter(&invert);
  sensor.publish_state(true);
  sensor.publish_initial_state(true);
  ASSERT_TRUE(sensor.has_state());
  EXPECT_FALSE(sensor.state);
}

}  // namespace esphome::binary_sensor::testing
