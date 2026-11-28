#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/scheduler.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <thread>
#include <tuple>

namespace esphome::core::testing {

class DestructorComponent : public Component {};

// A boxed callable whose destructor calls the scheduler; moving hands that duty to the scheduler's copy
struct OnFree {
  std::function<void()> action;
  std::shared_ptr<int> token;
  OnFree(std::function<void()> a, std::shared_ptr<int> t) : action(std::move(a)), token(std::move(t)) {}
  OnFree(OnFree &&other) noexcept : action(std::move(other.action)), token(std::move(other.token)) {
    other.action = nullptr;
  }
  ~OnFree() {
    if (this->action)
      this->action();
  }
  void operator()() const {}
};

// Where the scheduler frees the callable
enum class FreedBy {
  FREED_BY_AFTER_RUN,
  FREED_BY_CANCELLED_IN_TO_ADD,
  FREED_BY_CANCELLED_IN_HEAP,
  FREED_BY_BULK_CLEANUP,
  FREED_BY_DEFER_QUEUE,
  FREED_BY_DONT_RUN,
};
// What the destructor does
enum class DestructorAction {
  DESTRUCTOR_ACTION_SCHEDULE_MANY,
  DESTRUCTOR_ACTION_CANCEL_TIMEOUT,
  DESTRUCTOR_ACTION_CANCEL_INTERVAL,
  DESTRUCTOR_ACTION_RESCHEDULE_OWN_KEY,
};

static constexpr int SCHEDULED = 32;  // more than to_add_ holds, so the destructor makes it grow
static constexpr uint32_t VICTIM_ID = 1000;

class SchedulerCaptureDestructor : public ::testing::TestWithParam<std::tuple<FreedBy, DestructorAction>> {
 protected:
  void run_for(uint32_t ms) {
    const uint32_t end = millis() + ms;
    while (static_cast<int32_t>(end - millis()) > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      this->scheduler.call(millis());
    }
  }

  Scheduler scheduler;
  DestructorComponent component;
};

// On the base scheduler these destructors ran with the lock held: the host mutex deadlocked, so a watchdog
// turns a hang into a failure
TEST_P(SchedulerCaptureDestructor, DestructorMayCallTheScheduler) {
  const auto [freed_by, action] = GetParam();
  std::atomic<bool> done{false};
  std::thread watchdog([&done]() {
    for (int i = 0; i < 500 && !done; i++)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!done) {
      std::fprintf(stderr, "Deadlock: a capture's destructor could not call the scheduler\n");
      std::_Exit(1);
    }
  });

  auto token = std::make_shared<int>(0);
  int scheduled_ran = 0;
  bool other_ran = false;
  int interval_runs = 0;
  int interval_runs_at_free = -1;
  bool rescheduled_ran = false;
  bool destructor_ran = false;

  this->scheduler.set_timeout(&this->component, "other", 50, [&other_ran]() { other_ran = true; });
  this->scheduler.set_interval(&this->component, "interval", 1, [&interval_runs]() { interval_runs++; });
  this->scheduler.process_to_add();

  Scheduler *s = &this->scheduler;
  Component *c = &this->component;
  std::function<void()> on_free = [&, s, c, action = action]() {
    destructor_ran = true;
    switch (action) {
      case DestructorAction::DESTRUCTOR_ACTION_SCHEDULE_MANY:
        for (uint32_t i = 0; i < SCHEDULED; i++)
          s->set_timeout(c, i, 1, [&scheduled_ran]() { scheduled_ran++; });
        break;
      case DestructorAction::DESTRUCTOR_ACTION_CANCEL_TIMEOUT:
        s->cancel_timeout(c, "other");
        break;
      case DestructorAction::DESTRUCTOR_ACTION_CANCEL_INTERVAL:
        interval_runs_at_free = interval_runs;
        s->cancel_interval(c, "interval");
        break;
      case DestructorAction::DESTRUCTOR_ACTION_RESCHEDULE_OWN_KEY:
        s->set_timeout(c, VICTIM_ID, 1, [&rescheduled_ran]() { rescheduled_ran = true; });
        break;
    }
  };
  OnFree victim(std::move(on_free), token);

  switch (freed_by) {
    case FreedBy::FREED_BY_AFTER_RUN:
      this->scheduler.set_timeout(c, VICTIM_ID, 1, std::move(victim));
      break;
    case FreedBy::FREED_BY_CANCELLED_IN_TO_ADD:
      this->scheduler.set_timeout(c, VICTIM_ID, 10000, std::move(victim));
      this->scheduler.cancel_timeout(c, VICTIM_ID);
      this->scheduler.process_to_add();  // recycled while merging to_add_
      break;
    case FreedBy::FREED_BY_CANCELLED_IN_HEAP:
      this->scheduler.set_timeout(c, VICTIM_ID, 1, std::move(victim));
      this->scheduler.process_to_add();
      this->scheduler.cancel_timeout(c, VICTIM_ID);  // recycled by cleanup_() or the run loop
      break;
    case FreedBy::FREED_BY_BULK_CLEANUP:
      // "other" and "interval" stay at the top of the heap, so the removed items below need a full cleanup
      this->scheduler.set_timeout(c, VICTIM_ID, 10000, std::move(victim));
      for (uint32_t i = 1; i <= 5; i++)
        this->scheduler.set_timeout(c, VICTIM_ID + i, 10000, [token]() {});
      this->scheduler.process_to_add();
      for (uint32_t i = 0; i <= 5; i++)
        this->scheduler.cancel_timeout(c, VICTIM_ID + i);
      break;
    case FreedBy::FREED_BY_DEFER_QUEUE:
      this->scheduler.set_timeout(c, VICTIM_ID, 0, std::move(victim));
      break;
    case FreedBy::FREED_BY_DONT_RUN:
      this->scheduler.set_timeout(c, VICTIM_ID, SCHEDULER_DONT_RUN, std::move(victim));
      break;
  }

  this->run_for(80);
  EXPECT_TRUE(destructor_ran);
  switch (action) {
    case DestructorAction::DESTRUCTOR_ACTION_SCHEDULE_MANY:
      EXPECT_EQ(scheduled_ran, SCHEDULED);
      break;
    case DestructorAction::DESTRUCTOR_ACTION_CANCEL_TIMEOUT:
      EXPECT_FALSE(other_ran);
      break;
    case DestructorAction::DESTRUCTOR_ACTION_CANCEL_INTERVAL:
      EXPECT_GE(interval_runs_at_free, 0);
      EXPECT_EQ(interval_runs, interval_runs_at_free);
      break;
    case DestructorAction::DESTRUCTOR_ACTION_RESCHEDULE_OWN_KEY:
      EXPECT_TRUE(rescheduled_ran);
      break;
  }
  EXPECT_EQ(token.use_count(), 1);  // every capture was freed

  this->scheduler.cancel_interval(c, "interval");
  this->run_for(5);
  EXPECT_FALSE(this->scheduler.next_schedule_in(millis()).has_value());
  this->scheduler.trim_freelist();
  done = true;
  watchdog.join();
}

INSTANTIATE_TEST_SUITE_P(
    AllPaths, SchedulerCaptureDestructor,
    ::testing::Combine(::testing::Values(FreedBy::FREED_BY_AFTER_RUN, FreedBy::FREED_BY_CANCELLED_IN_TO_ADD,
                                         FreedBy::FREED_BY_CANCELLED_IN_HEAP, FreedBy::FREED_BY_BULK_CLEANUP,
                                         FreedBy::FREED_BY_DEFER_QUEUE, FreedBy::FREED_BY_DONT_RUN),
                       ::testing::Values(DestructorAction::DESTRUCTOR_ACTION_SCHEDULE_MANY,
                                         DestructorAction::DESTRUCTOR_ACTION_CANCEL_TIMEOUT,
                                         DestructorAction::DESTRUCTOR_ACTION_CANCEL_INTERVAL,
                                         DestructorAction::DESTRUCTOR_ACTION_RESCHEDULE_OWN_KEY)));

class SchedulerPendingRelease : public ::testing::Test {
 protected:
  Scheduler scheduler;
  DestructorComponent component;
};

// A timer a destructor schedules during call() is merged before call() returns, so sleep planning sees it
TEST_F(SchedulerPendingRelease, TimerScheduledByADestructorIsVisibleAfterOneCall) {
  Scheduler *s = &this->scheduler;
  Component *c = &this->component;
  auto token = std::make_shared<int>(0);
  this->scheduler.set_timeout(c, VICTIM_ID, 1, OnFree([s, c]() { s->set_timeout(c, "late", 200, []() {}); }, token));
  this->scheduler.process_to_add();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  this->scheduler.call(millis());
  EXPECT_EQ(token.use_count(), 1);
  const auto next = this->scheduler.next_schedule_in(millis());
  ASSERT_TRUE(next.has_value());
  EXPECT_LE(*next, 200u);
  this->scheduler.cancel_timeout(c, "late");
  this->scheduler.call(millis());
}

// A capture recycled after call() (by a cancel merged in the component phase, or by next_schedule_in()'s own
// cleanup) asks for a tick right away, and that tick frees it
TEST_F(SchedulerPendingRelease, RecycledAfterCallAsksForATick) {
  Component *c = &this->component;
  auto token = std::make_shared<int>(0);
  this->scheduler.set_timeout(c, VICTIM_ID, 10000, OnFree([]() {}, token));
  this->scheduler.process_to_add();
  this->scheduler.call(millis());
  this->scheduler.cancel_timeout(c, VICTIM_ID);  // after the scheduler phase: next_schedule_in() cleans it up
  EXPECT_EQ(this->scheduler.next_schedule_in(millis()), optional<uint32_t>(0));
  this->scheduler.call(millis());
  EXPECT_EQ(token.use_count(), 1);
  EXPECT_FALSE(this->scheduler.next_schedule_in(millis()).has_value());

  this->scheduler.set_timeout(c, VICTIM_ID, 10000, OnFree([]() {}, token));
  this->scheduler.cancel_timeout(c, VICTIM_ID);
  this->scheduler.process_to_add();  // the component phase's merge recycles it
  EXPECT_EQ(this->scheduler.next_schedule_in(millis()), optional<uint32_t>(0));
  this->scheduler.call(millis());
  EXPECT_EQ(token.use_count(), 1);
}

}  // namespace esphome::core::testing
