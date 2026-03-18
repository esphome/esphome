#include <benchmark/benchmark.h>

#include "esphome/core/scheduler.h"
#include "esphome/core/hal.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 2000;

// --- Scheduler fast path: no work to do ---

static void Scheduler_Call_NoWork(benchmark::State &state) {
  Scheduler scheduler;
  uint32_t now = millis();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.call(now);
    }
    benchmark::DoNotOptimize(now);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_Call_NoWork);

// --- Scheduler with timers: call() when timers exist but aren't due ---

static void Scheduler_Call_TimersNotDue(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;

  // Add some timeouts far in the future
  for (int i = 0; i < 10; i++) {
    scheduler.set_timeout(&dummy_component, static_cast<uint32_t>(i), 1000000, []() {});
  }
  scheduler.process_to_add();

  uint32_t now = millis();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.call(now);
    }
    benchmark::DoNotOptimize(now);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_Call_TimersNotDue);

// --- Scheduler with 5 intervals firing every call ---

static void Scheduler_Call_5IntervalsFiring(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;
  int fire_count = 0;

  // Benchmarks the heap-based scheduler dispatch with 5 callbacks firing.
  // Uses monotonically increasing fake time so intervals reliably fire every call.
  // USE_BENCHMARK ifdef in component.h disables WarnIfComponentBlockingGuard
  // (fake now > real millis() would cause underflow in finish()).
  // interval=0 would cause an infinite loop (reschedules at same now).
  for (int i = 0; i < 5; i++) {
    scheduler.set_interval(&dummy_component, static_cast<uint32_t>(i), 1, [&fire_count]() { fire_count++; });
  }
  scheduler.process_to_add();

  uint32_t now = millis() + 100;

  for (auto _ : state) {
    scheduler.call(now);
    now++;
    benchmark::DoNotOptimize(fire_count);
  }
}
BENCHMARK(Scheduler_Call_5IntervalsFiring);

// --- Scheduler: set_timeout registration ---

static void Scheduler_SetTimeout(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.set_timeout(&dummy_component, static_cast<uint32_t>(i % 5), 1000, []() {});
    }
    scheduler.process_to_add();
    benchmark::DoNotOptimize(scheduler);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_SetTimeout);

// --- Scheduler: set_interval registration ---

static void Scheduler_SetInterval(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;
  // Number of distinct interval keys; controls how many unique timers exist
  // simultaneously and the drain cadence for process_to_add().
  static constexpr int kKeyCount = 5;

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.set_interval(&dummy_component, static_cast<uint32_t>(i % kKeyCount), 1000, []() {});
      // Drain to_add_ periodically to reflect production behavior where
      // process_to_add() runs each main loop iteration. Without this,
      // cancelled items accumulate in to_add_ causing O(n²) scan cost.
      if ((i + 1) % kKeyCount == 0) {
        scheduler.process_to_add();
      }
    }
    // Final drain in case kInnerIterations is not a multiple of 5
    scheduler.process_to_add();
    benchmark::DoNotOptimize(scheduler);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_SetInterval);

// --- Scheduler: defer registration (set_timeout with delay=0) ---

static void Scheduler_Defer(benchmark::State &state) {
  Scheduler scheduler;
  Component dummy_component;

  // defer() is Component::defer which calls set_timeout(delay=0).
  // Call set_timeout directly since defer() is protected.
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      scheduler.set_timeout(&dummy_component, static_cast<uint32_t>(i % 5), 0, []() {});
    }
    scheduler.process_to_add();
    benchmark::DoNotOptimize(scheduler);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Scheduler_Defer);

}  // namespace esphome::benchmarks
