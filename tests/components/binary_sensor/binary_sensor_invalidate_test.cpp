#include <gtest/gtest.h>

#include <string>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/binary_sensor/filter.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::binary_sensor::testing {

static void run_scheduler_after(uint32_t ms) {
  delay(ms);
  App.scheduler.call(millis());
}

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

  // publish_initial_state() invalidates first, so a repeated value passes there too
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
  run_scheduler_after(30);
  EXPECT_FALSE(sensor.has_state());
}

// Mirrors the code generated for `delayed_on_off: 10ms`
static optional<bool> delayed_on_off_10ms(Filter *filter, bool x) { return delay_output(filter, 10, x); }

TEST(BinarySensorInvalidate, PendingApplyFilterOutputIsDropped) {
  BinarySensor sensor;
  ApplyFilter<delayed_on_off_10ms> delayed_on_off;
  sensor.add_filter(&delayed_on_off);
  sensor.publish_state(true);
  sensor.invalidate_state();
  run_scheduler_after(30);
  EXPECT_FALSE(sensor.has_state());

  // Without the invalidate the delayed output lands
  sensor.publish_state(true);
  run_scheduler_after(30);
  ASSERT_TRUE(sensor.has_state());
  EXPECT_TRUE(sensor.state);
}

TEST(BinarySensorInvalidate, SettleFilterStartsSteadyAfterInvalidate) {
  BinarySensor sensor;
  SettleFilter settle;
  settle.set_delay([]() -> uint32_t { return 10; });
  sensor.add_filter(&settle);
  sensor.publish_state(true);
  EXPECT_TRUE(sensor.state);

  sensor.invalidate_state();
  sensor.publish_state(true);
  ASSERT_TRUE(sensor.has_state());
  EXPECT_TRUE(sensor.state);

  // A change inside the settle time is pending; invalidating drops it
  sensor.publish_state(false);
  EXPECT_TRUE(sensor.state);
  sensor.invalidate_state();
  run_scheduler_after(30);
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

  run_scheduler_after(30);
  EXPECT_FALSE(sensor.has_state());

  sensor.publish_state(false);
  ASSERT_TRUE(sensor.has_state());
  EXPECT_TRUE(sensor.state);

  // Cancel the timeout that publish armed before the sensor on this stack frame goes away
  sensor.invalidate_state();
}

// An on_release automation that invalidates the sensor resets the autorepeat filter while it is still outputting
TEST(BinarySensorInvalidate, AutorepeatResetFromItsOwnOutputStopsTheRepeat) {
  BinarySensor sensor;
  AutorepeatFilter<1> autorepeat({{10, 10, 10}});
  sensor.add_filter(&autorepeat);
  bool invalidate_on_release = true;
  sensor.add_on_state_callback([&sensor, &invalidate_on_release](bool state) {
    if (!state && invalidate_on_release) {
      invalidate_on_release = false;
      sensor.invalidate_state();
    }
  });
  sensor.publish_state(true);
  ASSERT_TRUE(sensor.state);

  // The repeat starts after 10 ms with a release, and that release invalidates the sensor
  for (int i = 0; i < 3; i++) {
    run_scheduler_after(10);
  }
  EXPECT_FALSE(sensor.has_state());
}

// A pending output in the first, middle or last filter is dropped
TEST(BinarySensorInvalidate, EveryFilterInTheChainIsReset) {
  for (int pending = 0; pending < 3; pending++) {
    BinarySensor sensor;
    InvertFilter inverts[3];
    DelayedOnFilter delayed_on;
    delayed_on.set_delay([]() -> uint32_t { return 10; });
    for (int i = 0; i < 3; i++)
      sensor.add_filter(i == pending ? static_cast<Filter *>(&delayed_on) : &inverts[i]);
    // Send the value that reaches the delay as true, so it holds an output
    sensor.publish_state(pending != 1);
    sensor.invalidate_state();
    run_scheduler_after(30);
    EXPECT_FALSE(sensor.has_state()) << "pending in filter " << pending;
  }
}

// Records its id and passes the value on
class RecordingFilter : public Filter {
 public:
  RecordingFilter(std::string *log, char id) : log_(log), id_(id) {}
  optional<bool> new_value(bool value) override {
    *this->log_ += this->id_;
    return value;
  }

 protected:
  std::string *log_;
  char id_;
};

TEST(BinarySensorFilterChain, ValuesPassEveryFilterInOrderBeforeTheSensor) {
  BinarySensor sensor;
  std::string log;
  RecordingFilter first(&log, '1');
  RecordingFilter second(&log, '2');
  RecordingFilter third(&log, '3');
  sensor.add_filter(&first);
  sensor.add_filter(&second);
  sensor.add_filter(&third);
  sensor.publish_state(true);
  EXPECT_EQ(log, "123");
  ASSERT_TRUE(sensor.has_state());
  EXPECT_TRUE(sensor.state);
}

}  // namespace esphome::binary_sensor::testing
