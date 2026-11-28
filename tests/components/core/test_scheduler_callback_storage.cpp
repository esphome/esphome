#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/scheduler.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace esphome::core::testing {

class CallbackStorageComponent : public Component {};

// N words of captures: INLINE_WORDS fit (12 bytes on the chips, 24 on this host), one more does not
template<size_t N> struct Words {
  uintptr_t w[N];
  void operator()() const {}
};

static_assert(SchedulerCallback::FITS_INLINE<Words<SchedulerCallback::INLINE_WORDS>>);
static_assert(!SchedulerCallback::FITS_INLINE<Words<SchedulerCallback::INLINE_WORDS + 1>>);
static_assert(!SchedulerCallback::FITS_INLINE<decltype([s = std::string()]() { (void) s; })>);

static void run_due(Scheduler &scheduler) {
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  scheduler.call(millis());
}

// Runs a callable capturing N words plus a pointer and returns what it saw
template<size_t N> static uintptr_t run_words() {
  Scheduler scheduler;
  CallbackStorageComponent component;
  uintptr_t seen = 0;
  Words<N> words{};
  words.w[0] = 1;
  words.w[N - 1] += 320;
  scheduler.set_timeout(&component, "words", 1, [words, out = &seen]() { *out = words.w[0] + words.w[N - 1]; });
  run_due(scheduler);
  scheduler.trim_freelist();
  return seen;
}

// One word short of INLINE_WORDS plus the pointer fills the inline slot exactly; INLINE_WORDS plus it is boxed
TEST(SchedulerCallbackStorage, InlineAndBoxedCallbacksSeeTheirCaptures) {
  EXPECT_EQ(run_words<SchedulerCallback::INLINE_WORDS - 1>(), 321u);
  EXPECT_EQ(run_words<SchedulerCallback::INLINE_WORDS>(), 321u);
}

// An inline interval keeps its own copy of the captures, so a mutable lambda's state carries over between runs
TEST(SchedulerCallbackStorage, InlineIntervalKeepsMutableState) {
  Scheduler scheduler;
  CallbackStorageComponent component;
  uint32_t last = 0;
  scheduler.set_interval(&component, "count", 1, [count = uint32_t{0}, out = &last]() mutable { *out = ++count; });
  for (int i = 0; i < 3; i++)
    run_due(scheduler);
  EXPECT_GE(last, 3u);
  scheduler.cancel_interval(&component, "count");
  scheduler.call(millis());
  scheduler.trim_freelist();
}

// A pooled item is reused for either storage, in either order; a box is freed once its timeout has run or been
// cancelled
TEST(SchedulerCallbackStorage, PooledItemsSwitchStorage) {
  Scheduler scheduler;
  CallbackStorageComponent component;
  auto token = std::make_shared<int>(0);
  int runs = 0;
  int *counter = &runs;

  scheduler.set_timeout(&component, "a", 1, [counter]() { ++*counter; });
  run_due(scheduler);
  scheduler.set_timeout(&component, "b", 1, [token, counter]() { ++*counter; });
  EXPECT_EQ(token.use_count(), 2);
  run_due(scheduler);
  EXPECT_EQ(token.use_count(), 1);
  scheduler.set_timeout(&component, "c", 1, [counter]() { ++*counter; });
  run_due(scheduler);
  EXPECT_EQ(runs, 3);

  scheduler.set_timeout(&component, "cancelled", 10000, [token]() {});
  scheduler.process_to_add();
  EXPECT_EQ(token.use_count(), 2);
  scheduler.cancel_timeout(&component, "cancelled");
  scheduler.call(millis());
  EXPECT_EQ(token.use_count(), 1);
  scheduler.trim_freelist();
}

// SCHEDULER_DONT_RUN makes no item, so the box must be freed right away
TEST(SchedulerCallbackStorage, DontRunFreesTheBox) {
  Scheduler scheduler;
  CallbackStorageComponent component;
  auto token = std::make_shared<int>(0);
  scheduler.set_timeout(&component, "never", SCHEDULER_DONT_RUN, [token]() {});
  EXPECT_EQ(token.use_count(), 1);
}

// A std::function handed in as an lvalue is copied into a box, as an rvalue moved; both run
TEST(SchedulerCallbackStorage, StdFunctionArgumentsStillWork) {
  Scheduler scheduler;
  CallbackStorageComponent component;
  int runs = 0;
  std::function<void()> f = [&runs]() { runs++; };
  scheduler.set_timeout(&component, "copy", 1, f);
  scheduler.set_timeout(&component, "move", 1, std::move(f));
  run_due(scheduler);
  EXPECT_EQ(runs, 2);
  scheduler.trim_freelist();
}

}  // namespace esphome::core::testing
