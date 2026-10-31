#include <gtest/gtest.h>

#include <type_traits>

#include "esphome/components/interval/interval_callback.h"

namespace esphome::interval::testing {

namespace {

int calls = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void count_call() { calls++; }

}  // namespace

TEST(IntervalCallback, UpdateCallsFnAtDataPriority) {
  calls = 0;
  IntervalCallback<count_call> interval;
  EXPECT_EQ(interval.get_setup_priority(), setup_priority::DATA);
  EXPECT_FALSE(HasLoopOverride<IntervalCallback<count_call>>::value);
  interval.set_update_interval(1000);
  EXPECT_EQ(interval.get_update_interval(), 1000u);

  interval.setup();
  EXPECT_EQ(calls, 0);
  interval.update();
  interval.update();
  EXPECT_EQ(calls, 2);
}

TEST(IntervalCallback, DelayedIntervalCallbackIsAnIntervalCallback) {
  // Only the delayed class overrides setup(), so a zero delay keeps Component::setup()
  EXPECT_TRUE((std::is_base_of_v<IntervalCallback<count_call>, DelayedIntervalCallback<count_call, 5000>>) );
  EXPECT_FALSE((HasLoopOverride<DelayedIntervalCallback<count_call, 5000>>::value));
  calls = 0;
  DelayedIntervalCallback<count_call, 5000> delayed;
  EXPECT_EQ(delayed.get_setup_priority(), setup_priority::DATA);
  delayed.update();
  EXPECT_EQ(calls, 1);
}

}  // namespace esphome::interval::testing
