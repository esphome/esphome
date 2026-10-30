#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/scheduler.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
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

// A cancelled timeout of a component that has since failed has to be counted as removed when call()
// reaches it, or the leftover count makes the next few cancellations trigger a full cleanup early.
TEST(Scheduler, CancelledItemOfFailedComponentIsCountedAsRemoved) {
  Scheduler scheduler;
  SchedulerTestComponent healthy;
  SchedulerTestComponent failing;

  // The healthy item is due first, so cleanup_() stops at it and the cancelled item is only reached in the run loop
  scheduler.set_timeout(&healthy, "first", 1, []() {});
  scheduler.set_timeout(&failing, "cancelled", 2, []() {});
  scheduler.process_to_add();
  scheduler.cancel_timeout(&failing, "cancelled");
  failing.mark_failed();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  scheduler.call(millis());

  // Four cancelled items behind a live one stay below the full cleanup threshold of five, so their callbacks are
  // released only once they reach the top of the heap
  auto token = std::make_shared<int>(0);
  scheduler.set_timeout(&healthy, "keep", 10000, []() {});
  static constexpr const char *NAMES[] = {"c0", "c1", "c2", "c3"};
  for (const char *name : NAMES)
    scheduler.set_timeout(&healthy, name, 20000, [token]() {});
  scheduler.process_to_add();
  for (const char *name : NAMES)
    scheduler.cancel_timeout(&healthy, name);
  ASSERT_EQ(token.use_count(), 5);
  scheduler.call(millis());
  EXPECT_EQ(token.use_count(), 5);

  scheduler.cancel_timeout(&healthy, "keep");
  scheduler.call(millis());
  EXPECT_EQ(token.use_count(), 1);
  scheduler.trim_freelist();
}

}  // namespace esphome::core::testing
