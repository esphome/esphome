#include <gtest/gtest.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/binary_sensor/filter.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

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

TEST(BinarySensorInvalidate, PendingDelayedOutputIsDropped) {
  BinarySensor sensor;
  DelayedOnFilter delayed_on;
  delayed_on.set_delay([]() -> uint32_t { return 10; });
  sensor.add_filter(&delayed_on);
  sensor.publish_state(true);
  sensor.invalidate_state();
  delay(30);
  App.scheduler.call(millis());
  EXPECT_FALSE(sensor.has_state());
}

// TimeoutFilter invalidates the sensor from its own scheduler callback, which resets that same filter
TEST(BinarySensorInvalidate, TimeoutFilterInvalidatesAndTheSameValuePassesAgain) {
  BinarySensor sensor;
  TimeoutFilter timeout;
  timeout.set_timeout_value([]() -> uint32_t { return 10; });
  InvertFilter invert;
  sensor.add_filters({&timeout, &invert});
  sensor.publish_state(false);
  ASSERT_TRUE(sensor.has_state());
  EXPECT_TRUE(sensor.state);

  delay(30);
  App.scheduler.call(millis());
  EXPECT_FALSE(sensor.has_state());

  sensor.publish_state(false);
  ASSERT_TRUE(sensor.has_state());
  EXPECT_TRUE(sensor.state);

  // That publish armed the timeout again; let it run while the sensor on this stack frame is still alive
  delay(30);
  App.scheduler.call(millis());
}

}  // namespace esphome::binary_sensor::testing
