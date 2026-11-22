#include <gtest/gtest.h>
#include <cmath>

#include "esphome/components/sensor/filter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::sensor::testing {

static float two() { return 2.0f; }
static float one() { return 1.0f; }
static float ten() { return 10.0f; }
static const TemplatableFn<float> ONE_ONLY[] = {one, {}};

TEST(SensorFilterChain, EveryFilterRunsInOrderThenTheSensorGetsTheValue) {
  Sensor sensor;
  OffsetFilter offset(ten);
  MultiplyFilter multiply(two);
  sensor.add_filter(&offset);
  sensor.add_filter(&multiply);
  sensor.publish_state(1.0f);
  EXPECT_FLOAT_EQ(sensor.state, 22.0f);
}

// filter_out reads the sensor's accuracy while it is not the last filter
TEST(SensorFilterChain, FilterBeforeTheLastFindsTheSensor) {
  Sensor sensor;
  sensor.set_accuracy_decimals(1);
  FilterOutValueFilter filter_out(ONE_ONLY);
  MultiplyFilter multiply(two);
  sensor.add_filter(&filter_out);
  sensor.add_filter(&multiply);
  sensor.publish_state(5.0f);
  sensor.publish_state(1.04f);  // rounds to 1.0 at one decimal, so it is dropped
  EXPECT_FLOAT_EQ(sensor.state, 10.0f);
  sensor.publish_state(1.2f);
  EXPECT_FLOAT_EQ(sensor.state, 2.4f);
}

// Inside an or: filter the inner filters end at the or: filter's join node, not at the sensor
TEST(SensorFilterChain, FilterInsideOrFindsTheSensor) {
  Sensor sensor;
  sensor.set_accuracy_decimals(1);
  FilterOutValueFilter filter_out(ONE_ONLY);
  OrFilter<1> or_filter(&filter_out);
  MultiplyFilter multiply(two);
  sensor.add_filter(&or_filter);
  sensor.add_filter(&multiply);
  sensor.publish_state(1.04f);
  EXPECT_FALSE(sensor.has_state());
  sensor.publish_state(3.0f);
  EXPECT_FLOAT_EQ(sensor.state, 6.0f);
}

}  // namespace esphome::sensor::testing
