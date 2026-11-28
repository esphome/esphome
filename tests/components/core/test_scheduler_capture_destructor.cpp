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
enum class FreedBy { AFTER_RUN, CANCELLED_IN_TO_ADD, CANCELLED_IN_HEAP, BULK_CLEANUP, DEFER_QUEUE, DONT_RUN };
// What the destructor does
enum class Action { SCHEDULE_MANY, CANCEL_TIMEOUT, CANCEL_INTERVAL, RESCHEDULE_OWN_KEY };

static constexpr int SCHEDULED = 32;  // more than to_add_ holds, so the destructor makes it grow
static constexpr uint32_t VICTIM_ID = 1000;

class SchedulerCaptureDestructor : public ::testing::TestWithParam<std::tuple<FreedBy, Action>> {
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
      case Action::SCHEDULE_MANY:
        for (uint32_t i = 0; i < SCHEDULED; i++)
          s->set_timeout(c, i, 1, [&scheduled_ran]() { scheduled_ran++; });
        break;
      case Action::CANCEL_TIMEOUT:
        s->cancel_timeout(c, "other");
        break;
      case Action::CANCEL_INTERVAL:
        interval_runs_at_free = interval_runs;
        s->cancel_interval(c, "interval");
        break;
      case Action::RESCHEDULE_OWN_KEY:
        s->set_timeout(c, VICTIM_ID, 1, [&rescheduled_ran]() { rescheduled_ran = true; });
        break;
    }
  };
  OnFree victim(std::move(on_free), token);

  switch (freed_by) {
    case FreedBy::AFTER_RUN:
      this->scheduler.set_timeout(c, VICTIM_ID, 1, std::move(victim));
      break;
    case FreedBy::CANCELLED_IN_TO_ADD:
      this->scheduler.set_timeout(c, VICTIM_ID, 10000, std::move(victim));
      this->scheduler.cancel_timeout(c, VICTIM_ID);
      this->scheduler.process_to_add();  // recycled while merging to_add_
      break;
    case FreedBy::CANCELLED_IN_HEAP:
      this->scheduler.set_timeout(c, VICTIM_ID, 1, std::move(victim));
      this->scheduler.process_to_add();
      this->scheduler.cancel_timeout(c, VICTIM_ID);  // recycled by cleanup_() or the run loop
      break;
    case FreedBy::BULK_CLEANUP:
      // "other" and "interval" stay at the top of the heap, so the removed items below need a full cleanup
      this->scheduler.set_timeout(c, VICTIM_ID, 10000, std::move(victim));
      for (uint32_t i = 1; i <= 5; i++)
        this->scheduler.set_timeout(c, VICTIM_ID + i, 10000, [token]() {});
      this->scheduler.process_to_add();
      for (uint32_t i = 0; i <= 5; i++)
        this->scheduler.cancel_timeout(c, VICTIM_ID + i);
      break;
    case FreedBy::DEFER_QUEUE:
      this->scheduler.set_timeout(c, VICTIM_ID, 0, std::move(victim));
      break;
    case FreedBy::DONT_RUN:
      this->scheduler.set_timeout(c, VICTIM_ID, SCHEDULER_DONT_RUN, std::move(victim));
      break;
  }

  this->run_for(80);
  EXPECT_TRUE(destructor_ran);
  switch (action) {
    case Action::SCHEDULE_MANY:
      EXPECT_EQ(scheduled_ran, SCHEDULED);
      break;
    case Action::CANCEL_TIMEOUT:
      EXPECT_FALSE(other_ran);
      break;
    case Action::CANCEL_INTERVAL:
      EXPECT_GE(interval_runs_at_free, 0);
      EXPECT_EQ(interval_runs, interval_runs_at_free);
      break;
    case Action::RESCHEDULE_OWN_KEY:
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

INSTANTIATE_TEST_SUITE_P(AllPaths, SchedulerCaptureDestructor,
                         ::testing::Combine(::testing::Values(FreedBy::AFTER_RUN, FreedBy::CANCELLED_IN_TO_ADD,
                                                              FreedBy::CANCELLED_IN_HEAP, FreedBy::BULK_CLEANUP,
                                                              FreedBy::DEFER_QUEUE, FreedBy::DONT_RUN),
                                            ::testing::Values(Action::SCHEDULE_MANY, Action::CANCEL_TIMEOUT,
                                                              Action::CANCEL_INTERVAL, Action::RESCHEDULE_OWN_KEY)));

}  // namespace esphome::core::testing
