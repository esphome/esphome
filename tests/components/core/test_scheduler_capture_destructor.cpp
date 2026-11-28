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
#include <utility>

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

// Fails the test within 5 s if it deadlocks (a capture's destructor run under the scheduler's mutex); stops and joins
// its thread when the test ends, assertion failures included
class DeadlockWatchdog {
 public:
  DeadlockWatchdog()
      : thread_([this]() {
          for (int i = 0; i < 500 && !this->done_; i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
          if (!this->done_) {
            std::fprintf(stderr, "Deadlock: a capture's destructor could not call the scheduler\n");
            std::_Exit(1);
          }
        }) {}
  ~DeadlockWatchdog() {
    this->done_ = true;
    this->thread_.join();
  }

 private:
  std::atomic<bool> done_{false};
  std::thread thread_;
};

class SchedulerCaptureDestructor : public ::testing::TestWithParam<std::tuple<FreedBy, DestructorAction>> {
 protected:
  // Runs for at least ms and at least a few passes, so a host pause cannot end it before follow-up timers run
  void run_for_(uint32_t ms) {
    const uint32_t end = millis() + ms;
    for (int passes = 0; passes < 5 || static_cast<int32_t>(end - millis()) > 0; passes++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      this->scheduler_.call(millis());
    }
  }

  DeadlockWatchdog watchdog_;
  Scheduler scheduler_;
  DestructorComponent component_;
};

// A capture's destructor that calls the scheduler must not run with its lock held: on host that would deadlock on
// the mutex, so a watchdog turns a hang into a failure
TEST_P(SchedulerCaptureDestructor, DestructorMayCallTheScheduler) {
  const auto [freed_by, action] = GetParam();

  auto token = std::make_shared<int>(0);
  int scheduled_ran = 0;
  bool other_ran = false;
  bool other_cancelled = false;
  int interval_runs = 0;
  int interval_runs_at_free = -1;
  bool rescheduled_ran = false;
  bool destructor_ran = false;

  // Due after the watchdog's budget, so only the destructor's cancel (or the end of the test) removes it
  this->scheduler_.set_timeout(&this->component_, "other", 10000, [&other_ran]() { other_ran = true; });
  this->scheduler_.set_interval(&this->component_, "interval", 1, [&interval_runs]() { interval_runs++; });
  this->scheduler_.process_to_add();

  Scheduler *s = &this->scheduler_;
  Component *c = &this->component_;
  std::function<void()> on_free = [&, s, c, action = action]() {
    destructor_ran = true;
    switch (action) {
      case DestructorAction::DESTRUCTOR_ACTION_SCHEDULE_MANY:
        for (uint32_t i = 0; i < SCHEDULED; i++)
          s->set_timeout(c, i, 1, [&scheduled_ran]() { scheduled_ran++; });
        break;
      case DestructorAction::DESTRUCTOR_ACTION_CANCEL_TIMEOUT:
        other_cancelled = s->cancel_timeout(c, "other");
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
      this->scheduler_.set_timeout(c, VICTIM_ID, 1, std::move(victim));
      break;
    case FreedBy::FREED_BY_CANCELLED_IN_TO_ADD:
      this->scheduler_.set_timeout(c, VICTIM_ID, 10000, std::move(victim));
      this->scheduler_.cancel_timeout(c, VICTIM_ID);
      this->scheduler_.process_to_add();  // recycled while merging to_add_
      break;
    case FreedBy::FREED_BY_CANCELLED_IN_HEAP:
      this->scheduler_.set_timeout(c, VICTIM_ID, 1, std::move(victim));
      this->scheduler_.process_to_add();
      this->scheduler_.cancel_timeout(c, VICTIM_ID);  // recycled by cleanup_() or the run loop
      break;
    case FreedBy::FREED_BY_BULK_CLEANUP:
      // "interval" stays at the top of the heap, so the removed items below need a full cleanup
      this->scheduler_.set_timeout(c, VICTIM_ID, 10000, std::move(victim));
      for (uint32_t i = 1; i <= 5; i++)
        this->scheduler_.set_timeout(c, VICTIM_ID + i, 10000, [token]() {});
      this->scheduler_.process_to_add();
      for (uint32_t i = 0; i <= 5; i++)
        this->scheduler_.cancel_timeout(c, VICTIM_ID + i);
      break;
    case FreedBy::FREED_BY_DEFER_QUEUE:
      this->scheduler_.set_timeout(c, VICTIM_ID, 0, std::move(victim));
      break;
    case FreedBy::FREED_BY_DONT_RUN:
      this->scheduler_.set_timeout(c, VICTIM_ID, SCHEDULER_DONT_RUN, std::move(victim));
      break;
  }

  this->run_for_(80);
  EXPECT_TRUE(destructor_ran);
  switch (action) {
    case DestructorAction::DESTRUCTOR_ACTION_SCHEDULE_MANY:
      EXPECT_EQ(scheduled_ran, SCHEDULED);
      break;
    case DestructorAction::DESTRUCTOR_ACTION_CANCEL_TIMEOUT:
      EXPECT_TRUE(other_cancelled);  // the destructor's cancel found the timer
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

  this->scheduler_.cancel_interval(c, "interval");
  this->scheduler_.cancel_timeout(c, "other");
  this->run_for_(5);
  EXPECT_FALSE(this->scheduler_.next_schedule_in(millis()).has_value());
  this->scheduler_.trim_freelist();
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
  void TearDown() override { this->scheduler_.trim_freelist(); }  // Scheduler does not free its pool

  DeadlockWatchdog watchdog_;
  Scheduler scheduler_;
  DestructorComponent component_;
};

// A timer a destructor schedules during call() is merged before call() returns, so sleep planning sees it
TEST_F(SchedulerPendingRelease, TimerScheduledByADestructorIsVisibleAfterOneCall) {
  Scheduler *s = &this->scheduler_;
  Component *c = &this->component_;
  auto token = std::make_shared<int>(0);
  this->scheduler_.set_timeout(c, VICTIM_ID, 1, OnFree([s, c]() { s->set_timeout(c, "late", 200, []() {}); }, token));
  this->scheduler_.process_to_add();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  this->scheduler_.call(millis());
  EXPECT_EQ(token.use_count(), 1);
  const auto next = this->scheduler_.next_schedule_in(millis());
  ASSERT_TRUE(next.has_value());
  EXPECT_LE(*next, 200u);
  this->scheduler_.cancel_timeout(c, "late");
  this->scheduler_.call(millis());
}

// A capture recycled after call() (by a cancel merged in the component phase, or by next_schedule_in()'s own
// cleanup) asks for a tick right away, and that tick frees it
TEST_F(SchedulerPendingRelease, RecycledAfterCallAsksForATick) {
  Component *c = &this->component_;
  auto token = std::make_shared<int>(0);
  this->scheduler_.set_timeout(c, VICTIM_ID, 10000, OnFree([]() {}, token));
  this->scheduler_.process_to_add();
  this->scheduler_.call(millis());
  this->scheduler_.cancel_timeout(c, VICTIM_ID);  // after the scheduler phase: next_schedule_in() cleans it up
  EXPECT_EQ(this->scheduler_.next_schedule_in(millis()), optional<uint32_t>(0));
  this->scheduler_.call(millis());
  EXPECT_EQ(token.use_count(), 1);
  EXPECT_FALSE(this->scheduler_.next_schedule_in(millis()).has_value());

  this->scheduler_.set_timeout(c, VICTIM_ID, 10000, OnFree([]() {}, token));
  this->scheduler_.cancel_timeout(c, VICTIM_ID);
  this->scheduler_.process_to_add();  // the component phase's merge recycles it
  EXPECT_EQ(this->scheduler_.next_schedule_in(millis()), optional<uint32_t>(0));
  this->scheduler_.call(millis());
  EXPECT_EQ(token.use_count(), 1);
}

}  // namespace esphome::core::testing
