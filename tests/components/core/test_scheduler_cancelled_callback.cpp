#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/scheduler.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace esphome::core::testing {

class SchedulerTestComponent : public Component {};

// On a multi-threaded build next_schedule_in() only sees items_, so a timeout a callback leaves in
// to_add_ would let a loop that sleeps on next_schedule_in() oversleep. The case that used to skip
// the merge is a heap item (a zero timeout would run from the defer queue instead) whose callback
// cancels its own item.
TEST(Scheduler, TimeoutAddedByACancelledCallbackIsVisibleAfterCall) {
  Scheduler scheduler;
  SchedulerTestComponent component;
  bool fired = false;

  scheduler.set_timeout(&component, "first", 1, [&]() {
    scheduler.cancel_timeout(&component, "first");
    scheduler.set_timeout(&component, "second", 100, [&fired]() { fired = true; });
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  scheduler.call(millis());

  auto next = scheduler.next_schedule_in(millis());
  ASSERT_TRUE(next.has_value());
  const uint32_t next_ms = next.has_value() ? *next : 0;
  EXPECT_GT(next_ms, 50u);
  EXPECT_LE(next_ms, 100u);
  EXPECT_FALSE(fired);

  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  scheduler.call(millis());
  EXPECT_TRUE(fired);
  EXPECT_FALSE(scheduler.next_schedule_in(millis()).has_value());
  scheduler.trim_freelist();
}

}  // namespace esphome::core::testing
