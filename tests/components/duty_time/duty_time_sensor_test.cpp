#include <gtest/gtest.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/duty_time/duty_time_sensor.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::duty_time_sensor::testing {

// A binary sensor source counts its first on period, although its first state fires no on_state callback
TEST(DutyTimeSensor, SourceCountsItsFirstOnPeriod) {
  binary_sensor::BinarySensor source;
  source.set_trigger_on_initial_state(false);
  sensor::Sensor last_duty_time;
  DutyTimeSensor duty_time;
  duty_time.set_last_duty_time_sensor(&last_duty_time);
  duty_time.set_restore(false);
  duty_time.set_sensor(&source);
  duty_time.setup();

  source.publish_state(true);
  EXPECT_TRUE(duty_time.is_running());
  source.publish_state(false);
  EXPECT_FALSE(duty_time.is_running());
  EXPECT_TRUE(last_duty_time.has_state());
}

// An unknown state is ignored, and the state after it, again a first state, still counts
TEST(DutyTimeSensor, SourceCountsAcrossAnUnknownState) {
  binary_sensor::BinarySensor source;
  source.set_trigger_on_initial_state(false);
  sensor::Sensor last_duty_time;
  DutyTimeSensor duty_time;
  duty_time.set_last_duty_time_sensor(&last_duty_time);
  duty_time.set_restore(false);
  duty_time.set_sensor(&source);
  duty_time.setup();

  source.publish_state(false);
  source.invalidate_state();
  source.publish_state(true);
  EXPECT_TRUE(duty_time.is_running());
  source.invalidate_state();
  EXPECT_TRUE(duty_time.is_running());
  source.publish_state(false);
  EXPECT_FALSE(duty_time.is_running());
  EXPECT_TRUE(last_duty_time.has_state());
}

}  // namespace esphome::duty_time_sensor::testing
